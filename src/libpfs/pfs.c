/*
 * pFS API
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "pfs.h"
#include "instance.h"
#include "entry.h"
#include "file.h"
#include "dir_cache.h"
#include "path.h"


/*---------------------------------------------------------------------
 * Method: pfs_open
 * Scope: Global Public
 *
 * Open a file after checking the flags. Creates it if necessary
 * link (file_id) -> new_id used for read/write here.
 * So that our file can get reclaimed while open and we still be able
 * to use the new link to update the entry if the file get dirty.
 *
 *---------------------------------------------------------------------*/

int pfs_open (struct pfs_instance * pfs,
	      const char * path,
	      int flags, mode_t mode)
{
  struct pfs_open_file * open_file = NULL;  
  struct pfs_path_info pi;
  int retval, i;
  struct pfs_entry * entry = NULL;
  char * prt_path = NULL;
  char * file_name;


  if (strlen (path) == 1 && strncmp (path, "/", 1) == 0)
    return -EISDIR;

  open_file = (struct pfs_open_file *) malloc (sizeof (struct pfs_open_file));
  open_file->ver = NULL;
  open_file->fd = 0;
  open_file->dirty = 0;
  open_file->read_only = 0;

  /* If the entry does not exist we create it along with the
     underlying file. */
  if ((retval = pfs_get_path_info (pfs, path, &pi)) != 0)
    {
      if (retval != -ENOENT)
	goto error;

      if (!(flags & O_CREAT)) {
	retval = -ENOENT;
	goto error;
      }
      
      prt_path = (char *) malloc (sizeof (char) * (strlen (path) + 1));
      strncpy (prt_path, path, strlen (path) + 1); 
      
      for (i = strlen (prt_path) - 1; i > 0 && prt_path[i] != '/'; i--);
      prt_path[i] = 0;
      file_name = prt_path + (i + 1);
      
      if (i == 0 ||
	  strstr (file_name, ":") != NULL) {
	retval = -EACCES;
	goto error;
      }
      if (strlen (file_name) > (PFS_NAME_LEN - 1)) {
	retval = -ENAMETOOLONG;
	goto error;
      }
      
      if (pfs_get_path_info (pfs, prt_path, &pi) != 0) {
	retval = -ENOENT;
	goto error;
      }
      
      /* We create the entry. */
      open_file->ver = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
      open_file->ver->vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
      strncpy (open_file->ver->vv->last_updt, pfs->sd_id, PFS_ID_LEN);
      open_file->ver->vv->len = 1;
      open_file->ver->vv->sd_id = (char **) malloc (sizeof (char *));
      open_file->ver->vv->sd_id[0] = (char *) malloc (PFS_ID_LEN);
      strncpy (open_file->ver->vv->sd_id[0], pfs->sd_id, PFS_ID_LEN);
      open_file->ver->vv->value = (uint32_t *) malloc (sizeof (uint32_t));
      open_file->ver->vv->value[0] = 1;
      open_file->ver->type = PFS_FIL;
      
      if ((open_file->fd = pfs_file_create (pfs, open_file->ver->dst_id, flags)) < 0) {
	retval = -EIO;
	goto error;
      }
      
      open_file->ver->st_mode = mode | S_IFREG;

      strncpy (open_file->id, open_file->ver->dst_id, PFS_ID_LEN);
      strncpy (open_file->grp_id, pi.grp_id, PFS_ID_LEN);
      strncpy (open_file->dir_id, pi.dst_id, PFS_ID_LEN);
      strncpy (open_file->file_name, file_name, PFS_NAME_LEN);
      open_file->dirty = 2;
      pi.is_main = 1;
      pi.st_mode = open_file->ver->st_mode;

      /* We call pfs_set_entry here to make the newly created file visible. */
      if (pfs_set_entry (pfs, open_file->grp_id, open_file->dir_id,
			 open_file->file_name, 1, open_file->ver) != 0) {
	retval = -EIO;
	goto error;
      }

      free (prt_path);
      prt_path = NULL;
    }
  
  else if (pi.type == PFS_DEL && !(flags & O_CREAT))
    {
      retval = -ENOENT;
      goto error;
    }

  else if (pi.type == PFS_DEL && (flags & O_CREAT) && pi.is_main == 0)
    {
      retval = -EACCES;
      goto error;
    }

  /* If O_CREAT and pi.is_main, we have to create the
     file if it is a DEL entry. */
  else if (pi.type == PFS_DEL && (flags & O_CREAT) && pi.is_main == 1)
    {
      if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
	retval = -ENOENT;
	goto error;
      }
      open_file->ver = pfs_cpy_ver (entry->ver[entry->main_idx]);
      pfs_free_entry (entry);
      entry = NULL;

      open_file->ver->type = PFS_FIL;
      if ((open_file->fd = pfs_file_create (pfs, open_file->ver->dst_id, flags)) <= 0 ||
	  pfs_vv_incr (pfs, open_file->ver->vv) != 0) {
	retval = -EIO;
	goto error;
      }
      
      open_file->ver->st_mode = mode | S_IFREG;

      strncpy (open_file->id, open_file->ver->dst_id, PFS_ID_LEN);
      strncpy (open_file->grp_id, pi.grp_id, PFS_ID_LEN);
      strncpy (open_file->dir_id, pi.dir_id, PFS_ID_LEN);
      strncpy (open_file->file_name, pi.name, PFS_NAME_LEN);
      open_file->dirty = 2;

      /* We call pfs_set_entry here to make the newly created file visible. */
      if (pfs_set_entry (pfs, open_file->grp_id, open_file->dir_id,
			 open_file->file_name, 1, open_file->ver) != 0) {
	retval = -EIO;
	goto error;
      }
    }

  else if ((flags & O_CREAT) && (flags & O_EXCL)) {
    retval = -EEXIST;
    goto error;
  }

  /* The file exist we try to open it. */
  else
    {
      if (pi.type == PFS_DIR) {
	retval = -EISDIR;
	goto error;
      }

      /* Check "Permissions". */
      if ((pi.type != PFS_FIL) ||
	  ((pi.is_main == 0 || (pi.st_mode & S_IWUSR) == 0) &&
	   ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_TRUNC)))) {
	retval = -EACCES;
	goto error;
      }
      
      if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
	retval = -ENOENT;
	goto error;
      }
      for (i = 0; i < entry->ver_cnt; i ++) {
	if (strncmp (entry->ver[i]->dst_id, pi.dst_id, PFS_ID_LEN) == 0) {
	  open_file->ver = pfs_cpy_ver (entry->ver[i]);
	}
      }
      pfs_free_entry (entry);
      entry = NULL;
      if (open_file->ver == NULL) {
	retval = -ENOENT;
	goto error;
      }
      
      if (pi.is_main == 1 && ((flags & O_WRONLY) || (flags & O_RDWR)))
	{
	  if (pfs_file_link_new_id (pfs, pi.dst_id, open_file->id) != 0 ||
	      (prt_path = pfs_mk_file_path (pfs, open_file->id)) == NULL ||
	      (open_file->fd = open (prt_path, flags)) < 0) {
	    if (open_file->fd < 0)
	      retval = open_file->fd;
	    else
	      retval = -EIO;
	    goto error;
	  }
	  open_file->dirty = 0;
	}
      else
	{
	  strncpy (open_file->id, open_file->ver->dst_id, PFS_ID_LEN);
	  if((prt_path = pfs_mk_file_path (pfs, open_file->id)) == NULL ||
	     (open_file->fd = open (prt_path, flags)) < 0) {
	    if (open_file->fd < 0)
	      retval = open_file->fd;
	    else
	      retval = -EIO;
	    goto error;
	  }
	  open_file->dirty = 2;
	}

      strncpy (open_file->ver->dst_id, open_file->id, PFS_ID_LEN);
      strncpy (open_file->grp_id, pi.grp_id, PFS_ID_LEN);
      strncpy (open_file->dir_id, pi.dir_id, PFS_ID_LEN);
      strncpy (open_file->file_name, pi.name, PFS_NAME_LEN);

      /* We don't need to call pfs_set_entry. File is already present. */

      free (prt_path);
      prt_path = NULL;
    }

  if (pi.is_main == 1 && ((flags & O_WRONLY) || (flags & O_RDWR)))
    open_file->read_only = 0;
  else
    open_file->read_only = 1;
  
  pfs_mutex_lock (&pfs->open_lock);
  open_file->prev = NULL;
  open_file->next = pfs->open_file;
  if (pfs->open_file != NULL)
    pfs->open_file->prev = open_file;
  pfs->open_file = open_file;
  pfs_mutex_unlock (&pfs->open_lock);
  
  return open_file->fd;

 error:
  if (prt_path != NULL)
    free (prt_path);
  if (open_file != NULL) {
    if (open_file->fd > 0)
      close (open_file->fd);
    if (open_file->ver != NULL)
      pfs_free_ver (open_file->ver);
    free (open_file);
  }

  printf ("PFS_LOG : PFS_OPEN\n");
  printf ("ERROR : retval %d\n", retval);

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_pwrite
 * Scope: Global Public
 * FINAL
 * write to file set dirty as 1
 *
 *---------------------------------------------------------------------*/

ssize_t pfs_pwrite (struct pfs_instance * pfs,
		    int pfs_fd,
		    const void * buf,
		    size_t len,
		    off_t offset)
{
  struct pfs_open_file * open_file;
  ssize_t lenw;

  pfs_mutex_lock (&pfs->open_lock);
  open_file = pfs->open_file;
  while (open_file != NULL && open_file->fd != pfs_fd)
    open_file = open_file->next;
  pfs_mutex_unlock (&pfs->open_lock);
  
  if (open_file == NULL)
    return -EBADF;
  
  if (open_file->read_only == 1)
    return -EACCES;  

  if (open_file->dirty == 0)
    open_file->dirty = 1;  

  if (lseek (open_file->fd, offset, SEEK_SET) != offset ||
      (lenw = write (open_file->fd, buf, len)) < 0)
    return -errno;
  
  return lenw;
}


/*---------------------------------------------------------------------
 * Method: pfs_pwrite
 * Scope: Global Public
 * FINAL
 * read from file
 *
 *---------------------------------------------------------------------*/

ssize_t pfs_pread (struct pfs_instance * pfs,
		  int pfs_fd,
		  void * buf,
		  size_t len,
		  off_t offset)
{
  struct pfs_open_file * open_file;
  ssize_t lenr;

  pfs_mutex_lock (&pfs->open_lock);
  open_file = pfs->open_file;
  while (open_file != NULL && open_file->fd != pfs_fd)
    open_file = open_file->next;
  pfs_mutex_unlock (&pfs->open_lock);
  
  if (open_file == NULL)
    return -EBADF;
  
  if (lseek (open_file->fd, offset, SEEK_SET) != offset ||
      (lenr = read (open_file->fd, buf, len)) < 0)
    return -errno;
  
  return lenr;
}


/*---------------------------------------------------------------------
 * Method: pfs_fsync
 * Scope: Global Public
 * FINAL
 * fsync file desc
 *
 *---------------------------------------------------------------------*/

int pfs_fsync (struct pfs_instance * pfs,
	       int pfs_fd)
{
  int retval = 0;
  struct pfs_open_file * open_file;

  pfs_mutex_lock (&pfs->open_lock);
  open_file = pfs->open_file;
  while (open_file != NULL && open_file->fd != pfs_fd)
    open_file = open_file->next;
  pfs_mutex_unlock (&pfs->open_lock);
  
  if (open_file == NULL)
    return -EBADF;
  
  if (fsync (open_file->fd) < 0)
    return -errno;
  
  if (open_file->dirty == 2)
    retval = pfs_write_back_dir_cache (pfs, open_file->dir_id);
  
  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_close
 * Scope: Global Public
 *
 * Gets open_file in pfs->open_file DLL.
 *
 *---------------------------------------------------------------------*/

int pfs_close (struct pfs_instance * pfs,
	       int pfs_fd)
{
  int retval;
  struct pfs_open_file * open_file;

  pfs_mutex_lock (&pfs->open_lock);

  open_file = pfs->open_file;
  while (open_file != NULL && open_file->fd != pfs_fd)
    open_file = open_file->next;
  
  if (open_file == NULL) {
    pfs_mutex_unlock (&pfs->open_lock);
    return -EBADF;
  }

  if (open_file->prev != NULL)
    open_file->prev->next = open_file->next;
  else
    pfs->open_file = open_file->next;
  if (open_file->next != NULL)
    open_file->next->prev = open_file->prev;

  pfs_mutex_unlock (&pfs->open_lock);

  if ((retval = close (open_file->fd)) < 0) {
    pfs_free_ver (open_file->ver);
    free (open_file);
    return -errno;
  }

  if (open_file->dirty == 2) {
    /*
     * NOTHING TO DO
     * The file was created ver is up to date
     */
  }
  else if (open_file->dirty == 1) {
    pfs_vv_incr (pfs, open_file->ver->vv);
    if (pfs_set_entry (pfs, open_file->grp_id, open_file->dir_id, 
		       open_file->file_name, 1, open_file->ver) != 0) {
      pfs_free_ver (open_file->ver);
      free (open_file);
      return -EIO;
    }
  }
  else {
    if (pfs_file_unlink (pfs, open_file->id) < 0) {
      pfs_free_ver (open_file->ver);
      free (open_file);
      return -errno;
    }
  }

  pfs_free_ver (open_file->ver);
  free (open_file);

  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_stat
 * Scope: Global Public
 *
 * For now using underlying storage stat except for permission.
 *
 *---------------------------------------------------------------------*/

int pfs_stat (struct pfs_instance * pfs,
	      const char * path,
	      struct stat * stbuf)
{
  struct pfs_path_info pi;
  int retval;
  char * file_path = NULL;

  memset (stbuf, 0, sizeof (struct stat));
  if (strlen (path) == 1 && strncmp (path, "/", 1) == 0) {
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 2;
    return 0;
  }

  if ((retval = pfs_get_path_info (pfs, path, &pi)) != 0)
    goto error;

  /* We handle group directory separately. */
  if (pi.type == PFS_GRP)
    {
      file_path = pfs_mk_dir_path (pfs, pi.dst_id);
      if (file_path == NULL) {
	retval = -EIO;
	goto error;
      }
      if (stat (file_path, stbuf) < 0) {
	retval = -errno;
	goto error;
      }
      free (file_path);
      file_path = NULL;
      stbuf->st_mode = pi.st_mode;
      return 0;
    }

  if (pi.type == PFS_SML) {
    /*
     * TODO : SML
     */
    retval = -ENOENT;
    goto error;
  }

  if (pi.type == PFS_DEL) {
    retval = -ENOENT;
    goto error;
  }

  if (pi.type == PFS_FIL)
    file_path = pfs_mk_file_path (pfs, pi.dst_id);
  if (pi.type == PFS_DIR)
    file_path = pfs_mk_dir_path (pfs, pi.dst_id);

  if (file_path == NULL) {
    retval = -EIO;
    goto error;
  }
  
  if (stat (file_path, stbuf) < 0) {
    retval = -errno;
    goto error;
  }
  free (file_path);
  file_path = NULL;

  stbuf->st_mode = pi.st_mode;
  if (!pi.is_main) {
    stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
  }

  return 0;

 error:
  if (file_path != NULL)
    free (file_path);

  if (retval != -ENOENT) {
    printf ("PFS_LOG : PFS_STAT\n");
    printf ("ERROR : retval %d\n", retval);
  }

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_truncate
 * Scope: Global Public
 *
 * Truncate a file denoted by its fd
 *
 *---------------------------------------------------------------------*/

int pfs_ftruncate (struct pfs_instance * pfs,
		   int pfs_fd,
		   off_t len)
{
  struct pfs_open_file * open_file;

  pfs_mutex_lock (&pfs->open_lock);
  open_file = pfs->open_file;
  while (open_file != NULL && open_file->fd != pfs_fd)
    open_file = open_file->next;
  pfs_mutex_unlock (&pfs->open_lock);

  if (open_file == NULL)
    return -EBADF;
  
  if (open_file->read_only == 1)
    return -EIO;

  open_file->dirty = 1;  
  
  if (ftruncate (open_file->fd, (off_t) len) < 0)
    return -errno;

  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_truncate
 * Scope: Global Public
 *
 * Truncate a file denoted by path
 *
 *---------------------------------------------------------------------*/

int pfs_truncate (struct pfs_instance * pfs,
		  const char * path,
		  off_t len)
{
  int fd;

  /*
   * TODO : optimize
   */

  if ((fd = pfs_open (pfs, path, O_WRONLY, 0)) < 0)
    return -errno;
  
  if (pfs_ftruncate (pfs, fd, 0) < 0)
    return -errno;
  
  if (pfs_close (pfs, fd) < 0)
    return -errno;

  return 0;
}



/*---------------------------------------------------------------------
 * Method: pfs_readdir
 * Scope: Global Public
 *
 * Return a null terminated array of null terminated string representing
 * the directory entry.
 * Assumes that no object is bigger than PFS_NAME_LEN - 1
 * To be ensured in create_user/sd and file.
 *
 *---------------------------------------------------------------------*/

char ** pfs_readdir (struct pfs_instance * pfs,
		     const char * path)
{
  struct pfs_dir * dir = NULL;
  char ** entry_list = NULL;
  int i, j, k, entry_cnt;

  struct pfs_path_info pi;
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];

  struct pfs_info_group * next_grp;

  /* list the groups. */

  if (strlen (path) == 1 && strcmp (path, "/") == 0)
    {
      entry_list = (char **) malloc (sizeof (char *) * (pfs->grp_cnt + 3));
      for (i = 0; i < pfs->grp_cnt + 3; i ++)
	entry_list[i] = NULL;
      
      entry_list[0] = (char *) malloc (sizeof (char) * 2);
      strncpy (entry_list[0], ".", 2);
      entry_list[1] = (char *) malloc (sizeof (char) * 3);
      strncpy (entry_list[1], "..", 3);
      
      next_grp = pfs->group;
      for (i = 2; i < pfs->grp_cnt + 2; i ++) { 
	entry_list[i] = (char *) malloc (sizeof (char) * 
					 (strlen (next_grp->name) + 1));
	strncpy (entry_list[i], next_grp->name, strlen (next_grp->name) + 1);
	next_grp = next_grp->next;
    }
    
      entry_list[i] = NULL;
      return entry_list;
    }

  /* list a directory. */

  if (pfs_get_path_info (pfs, path, &pi) != 0)
    return NULL;

  dir = pfs_get_dir_cache (pfs, pi.dst_id);
  if (dir == NULL)
    return NULL;

  entry_cnt = 0;
  for (i = 0; i < dir->entry_cnt; i ++) {
    for (j = 0; j < dir->entry[i]->ver_cnt; j ++) {
      if (dir->entry[i]->ver[j]->type != PFS_DEL)
	entry_cnt ++;
    }
  }
  
  entry_list = (char **) malloc (sizeof (char *) * (entry_cnt + 3));
  for (i = 0; i < entry_cnt + 3; i ++)
    entry_list[i] = NULL;
  
  entry_list[0] = (char *) malloc (2);
  strncpy (entry_list[0], ".", 2);
  entry_list[1] = (char *) malloc (3);
  strncpy (entry_list[1], "..", 3);
  
  k = 2;
  for (i = 0; i < dir->entry_cnt; i ++) {
    for (j = 0; j < dir->entry[i]->ver_cnt; j ++) {
      if (dir->entry[i]->ver[j]->type != PFS_DEL) {
	if (pfs_get_sd_info (pfs, pi.grp_id, 
			     dir->entry[i]->ver[j]->vv->last_updt,
			     sd_owner, sd_name) != 0)
	  goto error;       
	if (j != dir->entry[i]->main_idx) {
	  entry_list[k] = (char *) malloc (sizeof (char) * 
					   (strlen (dir->entry[i]->name) +
					    strlen (sd_owner) +
					    strlen (sd_name) +
					    3));
	  sprintf (entry_list[k], "%s.%s:%s", 
		   sd_owner, sd_name, dir->entry[i]->name);
	}	
	else {
	  entry_list[k] = (char *) malloc (sizeof (char) * 
					   (strlen (dir->entry[i]->name) + 1));
	  strncpy (entry_list[k], dir->entry[i]->name, 
		   strlen (dir->entry[i]->name) + 1);
	}
	k ++;
      }
    }
  }

  pfs_unlock_dir_cache (pfs, dir);
  entry_list [k] = 0;

  return entry_list;

 error:
  if (dir != NULL)
    pfs_unlock_dir_cache (pfs, dir);
  if (entry_list != NULL) {
    for (i = 0; i < entry_cnt + 3; i ++) {
      if (entry_list[i] != NULL)
	free (entry_list[i]);
    }
    free (entry_list);
  }

  printf ("PFS_LOG : PFS_READDIR\n");
  printf ("ERROR\n");

  return NULL;
}



/*---------------------------------------------------------------------
 * Method: pfs_mkdir
 * Scope: Global Public
 *
 * Creates a directory.
 *
 *---------------------------------------------------------------------*/

int pfs_mkdir (struct pfs_instance * pfs,
	       const char * path,
	       mode_t mode)
{
  struct pfs_path_info pi;

  int i, retval;

  char * prt_path = NULL;
  char * name;

  struct pfs_ver * ver = NULL;
  struct pfs_entry * entry = NULL;
  
  //printf ("PFS_LOG : PFS_MKDIR\n");

  if (strlen (path) == 1 && strcmp (path, "/") == 0) {
    /*
     * TODO : Group creation
     */
    retval = -EACCES;
    goto error;
  }  
  
  /* The entry already exists. */
  if ((retval = pfs_get_path_info (pfs, path, &pi)) == 0)
    {
      if (pi.is_main != 1) {
	retval = -EACCES;
	goto error;
      }

      if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
	retval = -EIO;
	goto error;
      }
      
      if (entry->ver[entry->main_idx]->type != PFS_DEL) {
	retval = -EEXIST;
	goto error;
      }

      ver = pfs_cpy_ver (entry->ver[entry->main_idx]);
      pfs_free_entry (entry);
      entry = NULL;

      ver->type = PFS_DIR;
      ver->st_mode = mode | S_IFDIR;
      pfs_vv_incr (pfs, ver->vv);
      
      if (pfs_create_dir (pfs, ver->dst_id) != 0 ||
	  pfs_set_entry (pfs, pi.grp_id, pi.dir_id,
			 pi.name, 1, ver) != 0) {
	pfs_dir_rmdir (pfs, ver->dst_id);
	retval = -EIO;
	goto error;
      }

      pfs_free_ver (ver);
      ver = NULL;
      return 0; 
    }

  /* No entry. We have to create it. */
  else 
    {
      if (retval != -ENOENT)
	goto error;

      prt_path = (char *) malloc (sizeof (char) * (strlen (path) + 1));
      strncpy (prt_path, path, strlen (path) + 1); 
      
      for (i = strlen (prt_path) - 1; i > 0 && prt_path[i] != '/'; i--);
      prt_path[i] = 0;
      name = prt_path + (i + 1);
      
      if (i == 0 ||
	  strstr (name, ":") != NULL) {
	retval = -EACCES;
	goto error;
      }
      if (strlen (name) > (PFS_NAME_LEN - 1)) {
	retval = -ENAMETOOLONG;
	goto error;
      }
      
      if (pfs_get_path_info (pfs, prt_path, &pi) != 0) {
	retval = -ENOENT;
	goto error;
      }
      
      /* We create the entry. */
      
      ver = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
      ver->type = PFS_DIR;
      ver->st_mode = S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP 
	| S_IXGRP | S_IROTH | S_IXGRP | S_IFDIR;
      ver->vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
      memcpy (ver->vv->last_updt, pfs->sd_id, PFS_ID_LEN);
      ver->vv->len = 1;
      ver->vv->sd_id = (char **) malloc (sizeof (char *));
      ver->vv->sd_id[0] = (char *) malloc (PFS_ID_LEN);
      memcpy (ver->vv->sd_id[0], pfs->sd_id, PFS_ID_LEN);
      ver->vv->value = (uint32_t *) malloc (sizeof (uint32_t));
      ver->vv->value[0] = 1;

      if (pfs_create_dir (pfs, ver->dst_id) != 0 ||
	  pfs_set_entry (pfs, pi.grp_id, pi.dst_id, 
			 name, 1, ver) != 0) {
	pfs_dir_rmdir (pfs, ver->dst_id);
	retval = -EIO;
	goto error;
      }
      
      free (prt_path);
      pfs_free_ver (ver);
      prt_path = NULL;
      ver = NULL;
    }
  
  return 0;
  
 error:
  if (entry != NULL)
    pfs_free_entry (entry);
  if (ver != NULL)
    pfs_free_ver (ver);
  if (prt_path != NULL)
    free (prt_path);

  printf ("PFS_LOG : PFS_MKDIR\n");
  printf ("ERROR : retval %d\n", retval);

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_unlink
 * Scope: Global Public
 *
 * Unlink a file denoted by path. Only main version acceptable.
 *
 *---------------------------------------------------------------------*/

int pfs_unlink (struct pfs_instance * pfs,
		const char * path)
{
  struct pfs_path_info pi;
  struct pfs_entry * entry = NULL;
  struct pfs_ver * ver = NULL;
  int retval;

  if (strlen (path) == 1 && strcmp (path, "/") == 0) {
    retval = -EPERM;
    goto error;
  }

  if ((retval = pfs_get_path_info (pfs, path, &pi)) != 0)
      goto error;
  
  if (pi.type == PFS_GRP || pi.is_main != 1) {
    retval = -EACCES;
    goto error;
  }

  if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
    retval = -ENOENT;
    goto error;
  }

  if (entry->ver[entry->main_idx]->type == PFS_DIR) {
    retval = -EPERM;
    goto error;
  }

  ver = pfs_cpy_ver (entry->ver[entry->main_idx]);
  pfs_free_entry (entry);
  entry = NULL;

  ver->type = PFS_DEL;
  ver->st_mode = 0;
  memset (ver->dst_id, 0, PFS_ID_LEN);
  pfs_vv_incr (pfs, ver->vv);
  
  if (pfs_set_entry (pfs, pi.grp_id, pi.dir_id,
		     pi.name, 1, ver) != 0) {
    retval = -EIO;
    goto error;
  }
  
  pfs_free_ver (ver);
  ver = NULL;

  return 0;

 error:
  if (entry != NULL)
    pfs_free_entry (entry);
  if (ver != NULL)
    pfs_free_ver (ver);

  printf ("PFS_LOG : PFS_UNLINK\n");
  printf ("ERROR : retval %d\n", retval);

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_rmdir
 * Scope: Global Public
 *
 * Checks atomically if dir empty and remove it
 *
 *---------------------------------------------------------------------*/

int pfs_rmdir (struct pfs_instance * pfs,
	       const char * path)
{
  struct pfs_path_info pi;
  int retval;
  struct pfs_entry * entry = NULL;
  struct pfs_ver * ver = NULL;

  if (strlen (path) == 1 && strcmp (path, "/") == 0) {
    retval = -EACCES;
    goto error;
  }

  if ((retval = pfs_get_path_info (pfs, path, &pi)) != 0)
    goto error;

  if (pi.type != PFS_DIR || pi.is_main != 1) {
    retval = -EACCES;
    goto error;
  }

  if (pfs_dir_empty (pfs, pi.dst_id) == 0) {
    retval = -ENOTEMPTY;
    goto error;    
  }     

  if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
    retval = -ENOENT;
    goto error;
  }

  if (entry->ver[entry->main_idx]->type != PFS_DIR) {
    retval = -ENOTDIR;
    goto error;
  }

  ver = pfs_cpy_ver (entry->ver[entry->main_idx]);
  pfs_free_entry (entry);
  entry = NULL;

  ver->type = PFS_DEL;
  ver->st_mode = 0;
  memset (ver->dst_id, 0, PFS_ID_LEN);
  pfs_vv_incr (pfs, ver->vv);
  
  if ((retval = pfs_set_entry (pfs, pi.grp_id, pi.dir_id,
			       pi.name, 1, ver)) != 0) {
    retval = -EIO;
    goto error;
  }
  
  pfs_free_ver (ver);
  ver = NULL;

  //printf ("PFS_LOG : PFS_UNLINK DONE\n");

  return 0;

 error:
  if (entry != NULL)
    pfs_free_entry (entry);
  if (ver != NULL)
    pfs_free_ver (ver);

  if (retval != -ENOTEMPTY) {
    printf ("PFS_LOG : PFS_RMDIR\n");
    printf ("ERROR : retval %d\n", retval);
  }

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_rename
 * Scope: Global Public
 *
 * Rename a file or directory.
 *
 *---------------------------------------------------------------------*/

int pfs_rename (struct pfs_instance * pfs,
		const char * old,
		const char * new)
{

  struct pfs_path_info pi_old, pi_new;

  struct pfs_entry * entry = NULL;
  struct pfs_ver * ver_old = NULL;
  struct pfs_ver * ver_new = NULL;

  char * prt_path = NULL;
  char * name;
  int i, retval;

  if ((strlen (old) == 1 && strcmp (old, "/") == 0) || 
      (strlen (new) == 1 && strcmp (new, "/") == 0)) {
    retval = -EACCES;
    goto error;
  }

  if ((retval = pfs_get_path_info (pfs, old, &pi_old)) != 0)
    goto error;

  if (pi_old.type == PFS_GRP ||
      pi_old.is_main != 1) {
    retval = -EACCES;
    goto error;
  }

  if ((entry = pfs_get_entry (pfs, pi_old.dir_id, pi_old.name)) == NULL) {
    retval = -ENOENT;
    goto error;
  }

  ver_old = pfs_cpy_ver (entry->ver[entry->main_idx]);
  pfs_free_entry (entry);
  entry = NULL;

  if (pfs_get_path_info (pfs, new, &pi_new) == 0)
    {
      if (strncmp (pi_new.grp_id, pi_old.grp_id, PFS_ID_LEN) != 0) {
	retval = -EXDEV;
	goto error;
      }

      if (pi_new.type == PFS_GRP ||
	  pi_new.is_main != 1 ||
	  (pi_new.type != PFS_DEL && ((pi_new.st_mode & S_IWUSR) == 0))) {
	retval = -EACCES;
	goto error;
      }

      if (pi_new.type == PFS_DIR && pfs_dir_empty (pfs, pi_new.dst_id) == 0) {
	retval = -ENOTEMPTY;
	goto error;
      }     

      if ((entry = pfs_get_entry (pfs, pi_new.dir_id, pi_new.name)) == NULL) {
	retval = -EIO;
	goto error;
      }      
       
      ver_new = pfs_cpy_ver (entry->ver[entry->main_idx]);
      pfs_free_entry (entry);
      entry = NULL;

      pfs_vv_incr (pfs, ver_new->vv);  
    }

  else
    {      
      if (strncmp (pi_new.grp_id, pi_old.grp_id, PFS_ID_LEN) != 0) {
	retval = -EXDEV;
	goto error;
      }

      prt_path = (char *) malloc (sizeof (char) * (strlen (new) + 1));
      strncpy (prt_path, new, strlen (new) + 1); 
      
      for (i = strlen (prt_path) - 1; i > 0 && prt_path[i] != '/'; i--);
      prt_path[i] = 0;
      name = prt_path + (i + 1);
      
      if (i == 0 ||
	  strstr (name, ":") != NULL) {
	retval = -EACCES;
	goto error;
      }

      if (strlen (name) > (PFS_NAME_LEN - 1)) {
	retval = -ENAMETOOLONG;
	goto error;
      }

      if (pfs_get_path_info (pfs, prt_path, &pi_new) != 0) {
	retval = -ENOENT;
	goto error;
      }
      
      ver_new = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
      ver_new->vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
      memcpy (ver_new->vv->last_updt, pfs->sd_id, PFS_ID_LEN);
      ver_new->vv->len = 1;
      ver_new->vv->sd_id = (char **) malloc (sizeof (char *));
      ver_new->vv->sd_id[0] = (char *) malloc (PFS_ID_LEN);
      memcpy (ver_new->vv->sd_id[0], pfs->sd_id, PFS_ID_LEN);
      ver_new->vv->value = (uint32_t *) malloc (sizeof (uint32_t));
      ver_new->vv->value[0] = 1;
      memset (ver_new->dst_id, 0, PFS_ID_LEN);

      strncpy (pi_new.dir_id, pi_new.dst_id, PFS_ID_LEN);
      strncpy (pi_new.name, name, PFS_NAME_LEN);
      memset (pi_new.dst_id, 0, PFS_ID_LEN);

      free (prt_path);
      prt_path = NULL;
    }

  ver_new->type = ver_old->type;
  memcpy (ver_new->dst_id, ver_old->dst_id, PFS_ID_LEN);
  ver_new->st_mode = ver_old->st_mode;

  ver_old->type = PFS_DEL;
  memset (ver_old->dst_id, 0, PFS_ID_LEN);
  ver_old->st_mode = 0;
  pfs_vv_incr (pfs, ver_old->vv);
   
  if (pfs_set_entry (pfs, pi_old.grp_id, pi_old.dir_id,
		     pi_old.name, 0, ver_old) != 0) {
    retval = -EIO;
    goto error;
  }
   
  if (pfs_set_entry (pfs, pi_new.grp_id, pi_new.dir_id,
		     pi_new.name, 1, ver_new) != 0) {
    retval = -EIO;
    goto error;
  }
  
  pfs_free_ver (ver_old);
  pfs_free_ver (ver_new);
  
  return 0;

 error:
  if (entry != NULL)
    pfs_free_entry (entry);
  if (ver_old != NULL)
    pfs_free_ver (ver_old);
  if (ver_new != NULL)
    pfs_free_ver (ver_new);
  if (prt_path != NULL)
    free (prt_path);

  printf ("PFS_LOG : PFS_RENAME\n");
  printf ("ERROR : retval %d\n", retval);

  return retval;
}



/*---------------------------------------------------------------------
 * Method: pfs_statfs
 * Scope: Global Public
 *
 * Primarly used for now to pass the fs capacity
 *
 *---------------------------------------------------------------------*/

int pfs_statfs (struct pfs_instance * pfs,
		struct statvfs * buf)
{
  int retval;
  char * info_path;
  
  //printf ("PFS_LOG : PFS_STATFS\n");

  /* we use info file to get underlying fs stat. */
  info_path = (char *) malloc ((strlen (pfs->root_path) + strlen (PFS_INFO_PATH) + 1));
  if (info_path == NULL)
    return -EIO;
  sprintf (info_path, "%s%s", pfs->root_path, PFS_INFO_PATH);

  retval = statvfs (info_path, buf);
  free (info_path);

  buf->f_namemax = PFS_NAME_LEN - 1;
  buf->f_flag = ST_NOSUID;
  buf->f_fsid = 0;

  //printf ("PFS_LOG : PFS_STATFS DONE\n");

  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_readlink
 * Scope: Global Public
 *
 * 
 *
 *---------------------------------------------------------------------*/

int pfs_readlink (struct pfs_instance * pfs,
		  const char * path,
		  char * buf,
		  size_t bufsize)
{
  return -ENOENT;
}

int pfs_symlink (struct pfs_instance * pfs,
		 const char * from,
		 const char * to)
{  
  return -ENOENT;
}


/*---------------------------------------------------------------------
 * Method: pfs_utime
 * Scope: Global Public
 *
 * Set the tile access/modification time
 *
 *---------------------------------------------------------------------*/

int pfs_utimens (struct pfs_instance * pfs,
		 const char * path,
		 const struct timespec tv[2])
{						
  struct pfs_path_info pi;
  int retval;
  char * file_path = NULL;
  struct timeval arg [2];

  if (strlen (path) == 1 && strcmp (path, "/") == 0) {
    retval = -EACCES;
    goto error;
  }

  if (pfs_get_path_info (pfs, path, &pi) != 0) {
    retval = -ENOENT;
    goto error;
  }

  if (pi.type == PFS_GRP) {
    retval = -EACCES;
    goto error;
  }

  if (pi.type == PFS_SML) {
    /*
     * TODO SML
     */
    retval = -ENOENT;
    goto error;
  }

  if (pi.type == PFS_FIL)
    file_path = pfs_mk_file_path (pfs, pi.dst_id);
  if (pi.type == PFS_DIR)
    file_path = pfs_mk_dir_path (pfs, pi.dst_id);

  if (file_path == NULL) {
    retval = -EIO;
    goto error;
  }
  
  arg[0].tv_sec = tv[0].tv_sec;
  arg[0].tv_usec = tv[0].tv_nsec / 1000;
  arg[1].tv_sec = tv[1].tv_sec;
  arg[1].tv_usec = tv[1].tv_nsec / 1000;

  if (utimes (file_path, arg) < 0) {
    retval = -errno;
    goto error;
  }

  free (file_path);
  return 0;

 error:
  if (file_path != NULL)
    free (file_path);
  
  printf ("PFS_LOG : PFS_UTMIENS\n");
  printf ("ERROR : retval %d\n", retval);
  
  return retval;
}


/*---------------------------------------------------------------------
 * Method: pfs_chmod
 * Scope: Global Public
 *
 * Set the access mode
 *
 *---------------------------------------------------------------------*/

int pfs_chmod (struct pfs_instance * pfs,
	       const char * path,
	       mode_t mode)
{
  struct pfs_path_info pi;
  struct pfs_entry * entry = NULL;
  struct pfs_ver * ver = NULL;
  int retval;

  if (strlen (path) == 1 && strcmp (path, "/") == 0) {
    retval = -EPERM;
    goto error;
  }

  if ((retval = pfs_get_path_info (pfs, path, &pi)) != 0)
      goto error;
  
  if (pi.type == PFS_GRP || pi.is_main != 1) {
    retval = -EACCES;
    goto error;
  }

  if ((entry = pfs_get_entry (pfs, pi.dir_id, pi.name)) == NULL) {
    retval = -ENOENT;
    goto error;
  }

  ver = pfs_cpy_ver (entry->ver[entry->main_idx]);
  pfs_free_entry (entry);
  entry = NULL;

  ver->st_mode = mode;
  if (pi.type == PFS_DIR)
    ver->st_mode |= S_IFDIR;
  if (pi.type == PFS_FIL)
    ver->st_mode |= S_IFREG;

  pfs_vv_incr (pfs, ver->vv);
  
  if (pfs_set_entry (pfs, pi.grp_id, pi.dir_id,
		     pi.name, 0, ver) != 0) {
    retval = -EIO;
    goto error;
  }
  
  pfs_free_ver (ver);
  ver = NULL;

  return 0;

 error:
  if (entry != NULL)
    pfs_free_entry (entry);
  if (ver != NULL)
    pfs_free_ver (ver);

  printf ("PFS_LOG : PFS_UNLINK\n");
  printf ("ERROR : retval %d\n", retval);

  return retval;
}


/* SPECIAL OPERATIONS */

int pfs_create_group (struct pfs_instance * pfs,
		      const char * grp)
{
  return 0;
}

int pfs_add_sd (struct pfs_instance * pfs,
		const char * grp,
		const char * sd_owner,
		const char * sd_name,
		const char * sd_id)
{
  return 0;
}

int
pfs_bootstrap (const char * root_path,
	       const char * sd_owner,
	       const char * sd_name)
{
  return pfs_bootstrap_inst (root_path,
			     sd_owner,
			     sd_name);
}

struct pfs_instance *
pfs_init (const char * root_path)
{
  return pfs_init_instance (root_path);
}

int
pfs_destroy (struct pfs_instance * pfs)
{
  printf ("PFS_LOG : PFS_DESTROY\n");
  return pfs_destroy_instance (pfs);
}

int
pfs_sync_cache (struct pfs_instance * pfs)
{
  return pfs_sync_dir_cache (pfs);
}

int 
pfs_set_updt_cb (struct pfs_instance * pfs,
		     int(*updt_cb)(struct pfs_instance *, struct pfs_updt *))
{
  pfs->updt_cb = updt_cb;
  return 0;
}
