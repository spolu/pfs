#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include <sys/file.h>

#include "instance.h"
#include "entry.h"


static void mk_hash (char * from, size_t len, char * to);

/*---------------------------------------------------------------------
 * Method: pfs_init_instance
 * Scope:  Global
 *
 * Initiate pfs_instance structure using info and groups files.
 *
 *---------------------------------------------------------------------*/

struct pfs_instance *
pfs_init_instance (const char * root_path)
{
  struct pfs_instance * pfs = NULL;
  char * info_path;
  int fd = -1;

  pfs = (struct pfs_instance *) malloc (sizeof (struct pfs_instance));
  
  /* Set up the paths. */
  pfs->root_path = (char *) malloc ((strlen (root_path) + 1));
  strncpy (pfs->root_path, root_path, strlen (root_path) + 1);

  pfs->data_path = (char *) malloc ((strlen (root_path) + 
				     strlen (PFS_DATA_PATH) + 1));
  sprintf (pfs->data_path, "%s%s", root_path, PFS_DATA_PATH);

  pfs->sml_path = (char *) malloc ((strlen (root_path) + 
				    strlen (PFS_SML_PATH) + 1));
  sprintf (pfs->sml_path, "%s%s", root_path, PFS_SML_PATH);

  info_path = (char *) malloc ((strlen (root_path) + strlen (PFS_INFO_PATH) + 1));
  sprintf (info_path, "%s%s", root_path, PFS_INFO_PATH);

  ASSERT ((fd = open (info_path, O_RDONLY)) >= 0);
  readn (fd, pfs->sd_owner, PFS_NAME_LEN);
  readn (fd, pfs->sd_id, PFS_ID_LEN);
  readn (fd, pfs->sd_name, PFS_NAME_LEN);
  readn (fd, &pfs->uid_cnt, sizeof (uint32_t));
  close (fd);

  /* we use info file as lock file for group open_file list access. */
  pfs->open_file = NULL;
  pfs->group = NULL;
  pfs_mutex_init (&pfs->open_lock);
  
  free (info_path);
  
  ASSERT (pfs_read_group_info (pfs) == 0);

  pfs_init_dir_cache (pfs);

  pfs->updt_cb = NULL;

  /*
    printf ("\n*** INITIATING pfs_instance : ***\n");
    printf ("%s\n", pfs->sd_owner);
    printf ("%.*s\n", PFS_ID_LEN, pfs->sd_id);
    printf ("%s\n", pfs->sd_name);
    printf ("%d\n", pfs->uid_cnt);
    printf ("***\n\n");
  */

  return pfs;
}


/*---------------------------------------------------------------------
 * Method: pfs_destroy_instance
 * Scope:  Global
 *
 * Write back dynamic data as uid_cnt
 *
 *---------------------------------------------------------------------*/

int
pfs_destroy_instance (struct pfs_instance * pfs)
{
  char * info_path;
  int fd;

  /* We close all the files. */
  while (pfs->open_file != NULL) {
    pfs_close (pfs, pfs->open_file->fd);
  }

  pfs_destroy_dir_cache (pfs);
  pfs_free_group_info (pfs);

  info_path = (char *) malloc (strlen (pfs->root_path) + 
			       strlen (PFS_INFO_PATH) + 1);
  sprintf (info_path, "%s%s", pfs->root_path, PFS_INFO_PATH);

  if ((fd = open (info_path, O_WRONLY|O_TRUNC|O_APPEND)) < 0) {
    free (pfs);
    free (info_path);
    return -1;
  }
  free (info_path);

  writen (fd, pfs->sd_owner, PFS_NAME_LEN);
  writen (fd, pfs->sd_id, PFS_ID_LEN);
  writen (fd, pfs->sd_name, PFS_NAME_LEN);
  writen (fd, &pfs->uid_cnt, sizeof (uint32_t));
  close (fd);

  pfs_mutex_destroy (&pfs->open_lock);

  free (pfs->root_path);
  free (pfs->data_path);
  free (pfs->sml_path);
  free (pfs);

  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_mk_id
 * Scope:  Global
 *
 * Creates a unique id an place it in id using pfs instance.
 *
 *---------------------------------------------------------------------*/

int pfs_mk_id (struct pfs_instance * pfs,
		char * id)
{
  char data [2 * PFS_ID_LEN];

  pfs->uid_cnt++;
  memcpy (data, pfs->sd_id, PFS_ID_LEN);
  sprintf (data + PFS_ID_LEN, "%d", pfs->uid_cnt);
  mk_hash (data, strlen (data), id);
  return 0;
}



/*---------------------------------------------------------------------
 * Method: pfs_mk_file_path
 * Scope:  Global
 *
 * Generates a path to a file file given its id an an instance
 *
 *---------------------------------------------------------------------*/

char *
pfs_mk_file_path (struct pfs_instance * pfs,
		  const char * id)
{
  char * path;

  if (id[0] == 0) 
    return NULL;
  path = (char *) malloc (strlen (pfs->data_path) + 
			  PFS_ID_LEN + 5);
  sprintf (path, "%s%c/%c/%.*s", pfs->data_path, id[0], id[1], PFS_ID_LEN, id);
  return path;
}

/*---------------------------------------------------------------------
 * Method: pfs_mk_dir_path
 * Scope:  Global
 *
 * Generates a path to a dir file given its id an an instance
 *
 *---------------------------------------------------------------------*/

char *
pfs_mk_dir_path (struct pfs_instance * pfs,
		 const char * id)
{
  char * path;

  if (id[0] == 0) 
    return NULL;
  path = (char *) malloc (strlen (pfs->data_path) + 
			  PFS_ID_LEN + 3);
  sprintf (path, "%s%c/%c/%.*s", pfs->data_path, id[0], id[1], PFS_ID_LEN, id);
  return path;
}


/*---------------------------------------------------------------------
 * Method: pfs_get_v_sd_id
 * Scope: Global
 *
 * get the v_sd_id [local] associated with this grp_name and grp_id.
 *
 *---------------------------------------------------------------------*/

int pfs_get_v_sd_id (struct pfs_instance * pfs,
		     const char * grp_name,
		     char * grp_id,
		     char * v_sd_id)
{
  struct pfs_info_group * next_grp;
  uint8_t redo = 0;

 redo:
  if (pfs->group == NULL)
    return -1;
  next_grp = pfs->group;
  
  while (next_grp != NULL) {
    if (strncmp (grp_name, next_grp->name, PFS_NAME_LEN) == 0) {
      memcpy (grp_id, next_grp->id, PFS_ID_LEN);
      memcpy (v_sd_id, next_grp->v_sd_id, PFS_ID_LEN);
      return 0;
    }
    next_grp = next_grp->next;
  }
  
  if (redo == 1)
    return -1;

  /* We update the group_info mayber a user/group has been created. */
  pfs_free_group_info (pfs);
  pfs_read_group_info (pfs);
  redo = 1;

  goto redo;

  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_get_sd_id
 * Scope: Global
 *
 * get the sd_id [global] associated with this owner/sd. Eventually
 * reloads pfs->info if not found first time (concurrent user add)
 *
 *---------------------------------------------------------------------*/

int pfs_get_sd_id (struct pfs_instance * pfs,
		   const char * grp_id,
		   const char * sd_owner,
		   const char * sd_name,
		   char * sd_id)
{
  struct pfs_info_group * next_grp;
  struct pfs_info_sd * next_sd;
  uint8_t redo = 0;

 redo:
  if (pfs->group == NULL)
    return -1;
  next_grp = pfs->group;

  while (next_grp != NULL) {
    if (strncmp (grp_id, next_grp->id, PFS_ID_LEN) == 0) {
      next_sd = next_grp->sd;
      while (next_sd != NULL) {
	if (strncmp (sd_owner, next_sd->sd_owner, PFS_NAME_LEN) == 0 &&
	    strncmp (sd_name, next_sd->sd_name, PFS_NAME_LEN) == 0) {
	  memcpy (sd_id, next_sd->sd_id, PFS_ID_LEN);
	  return 0;
	}
	next_sd = next_sd->next;
      }
    }
    next_grp = next_grp->next;
  }

  if (redo == 1)
    return -1;

  /* We update the group_info mayber a user/group has been created. */
  pfs_free_group_info (pfs);
  pfs_read_group_info (pfs);
  redo = 1;

  goto redo;

  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_get_sd_info
 * Scope: Global
 *
 * get grp_id and sd_id and returns the sd_owner and sd_name
 *
 *---------------------------------------------------------------------*/

int pfs_get_sd_info (struct pfs_instance * pfs,
		     const char * grp_id,
		     const char * sd_id,
		     char * sd_owner,
		     char * sd_name)
{
  struct pfs_info_group * next_grp;
  struct pfs_info_sd * next_sd;
  uint8_t redo = 0;

 redo:
  if (pfs->group == NULL)
    return -1;
  next_grp = pfs->group;
  
  while (next_grp != NULL) {
    if (strncmp (grp_id, next_grp->id, PFS_ID_LEN) == 0) {
      next_sd = next_grp->sd;
      while (next_sd != NULL) {
	if (strncmp (sd_id, next_sd->sd_id, PFS_ID_LEN) == 0) {
	  strncpy (sd_owner, next_sd->sd_owner, PFS_NAME_LEN);
	  strncpy (sd_name, next_sd->sd_name, PFS_NAME_LEN);
	  return 0;
	}
	next_sd = next_sd->next;
      }
    }
    next_grp = next_grp->next;
  }

  if (redo == 1)
    return -1;

  /* We update the group_info mayber a user/group has been created. */
  pfs_free_group_info (pfs);
  pfs_read_group_info (pfs);
  redo = 1;

  goto redo;

  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_(read|free|write)_group_info
 * Scope:  Global
 *
 * Load or free info field of pfs instance. Used to initially load info
 * or reload when a group info hasn't been found.
 *
 *---------------------------------------------------------------------*/

int
pfs_read_group_info (struct pfs_instance * pfs)
{
  int fd;
  int i,j;
  struct pfs_info_group * new_grp;
  struct pfs_info_sd * new_sd;
  char * group_info_path;  

  group_info_path = (char *) malloc (strlen (pfs->root_path) + 
				     strlen (PFS_GROUP_INFO_PATH) + 1);
  sprintf (group_info_path, "%s%s", pfs->root_path, PFS_GROUP_INFO_PATH);

  if ((fd = open (group_info_path, O_RDONLY)) < 0) {
    return -1;
  }
  flock (fd, LOCK_SH);

  readn (fd, &pfs->grp_cnt, sizeof (uint32_t));

  for (i = 0; i < pfs->grp_cnt; i ++) {
    new_grp = (struct pfs_info_group *) malloc (sizeof (struct pfs_info_group));
    readn (fd, new_grp->id, PFS_ID_LEN);
    readn (fd, new_grp->name, PFS_NAME_LEN);
    readn (fd, new_grp->v_sd_id, PFS_ID_LEN);
    readn (fd, &new_grp->sd_cnt, sizeof (uint32_t));
    new_grp->sd = NULL;
    
    for (j = 0; j < new_grp->sd_cnt; j ++) {
      new_sd = (struct pfs_info_sd *) malloc (sizeof (struct pfs_info_sd));
      readn (fd, new_sd->sd_id, PFS_ID_LEN);
      readn (fd, new_sd->sd_owner, PFS_NAME_LEN);
      readn (fd, new_sd->sd_name, PFS_NAME_LEN);

      new_sd->next = new_grp->sd;
      new_grp->sd = new_sd;
    }

    new_grp->next = pfs->group;
    pfs->group = new_grp;
  }

  free (group_info_path);
  flock (fd, LOCK_UN);
  close (fd);
  return 0;
}


int
pfs_free_group_info (struct pfs_instance * pfs)
{
  struct pfs_info_group * next_group;
  struct pfs_info_sd * next_sd;

  while (pfs->group != NULL) {
    next_group = pfs->group->next;
    while (pfs->group->sd != NULL) {
      next_sd = pfs->group->sd->next;
      free (pfs->group->sd);
      pfs->group->sd = next_sd;
    }
    free (pfs->group);
    pfs->group = next_group;
  }
  pfs->group = NULL;

  return 0;
}

int
pfs_write_group_info (struct pfs_instance * pfs)
{
  int fd;
  int i,j;
  struct pfs_info_group * next_grp;
  struct pfs_info_sd * next_sd;
  char * group_info_path;  

  group_info_path = (char *) malloc (strlen (pfs->root_path) + 
				     strlen (PFS_GROUP_INFO_PATH) + 1);
  sprintf (group_info_path, "%s%s", pfs->root_path, PFS_GROUP_INFO_PATH);
  
  if (pfs->group == NULL)
    return -1;

  if ((fd = open (group_info_path, O_WRONLY|O_APPEND|O_TRUNC|O_CREAT)) < 0)
    return -1;
  flock (fd, LOCK_EX);

  writen (fd, &pfs->grp_cnt, sizeof (uint32_t));
  next_grp = pfs->group;
  
  for (i = 0; i < pfs->grp_cnt; i ++) {
    writen (fd, next_grp->id, PFS_ID_LEN);
    writen (fd, next_grp->name, PFS_NAME_LEN);
    writen (fd, next_grp->v_sd_id, PFS_ID_LEN);
    writen (fd, &next_grp->sd_cnt, sizeof (uint32_t));

    next_sd = next_grp->sd;    
    for (j = 0; j < next_grp->sd_cnt; j ++) {
      writen (fd, next_sd->sd_id, PFS_ID_LEN);
      writen (fd, next_sd->sd_owner, PFS_NAME_LEN);
      writen (fd, next_sd->sd_name, PFS_NAME_LEN);

      next_sd = next_sd->next;
    }

    next_grp = next_grp->next;
  }

  free (group_info_path);
  fchmod (fd, S_IRUSR | S_IWUSR);
  flock (fd, LOCK_UN);
  close (fd);
  
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_bootstrap_inst
 * Scope:  Global
 *
 * Bootstraps pfs file system in root_path with sd_owner and sd_name
 *
 *---------------------------------------------------------------------*/

int pfs_bootstrap_inst (const char * root_path,
			const char * sd_owner,
			const char * sd_name)
{
  char data [2 * PFS_NAME_LEN + 2];
  struct pfs_instance * pfs = NULL;
  char * info_path;
  int fd, i, j;
  char * data_subdir;

  pfs = (struct pfs_instance *) malloc (sizeof (struct pfs_instance));

  pfs->root_path = (char *) malloc (strlen (root_path) + 1);
  strncpy (pfs->root_path, root_path, strlen (root_path) + 1);

  pfs->data_path = (char *) malloc (strlen (pfs->root_path) + 
				    strlen (PFS_DATA_PATH) + 1);
  sprintf (pfs->data_path, "%s%s", pfs->root_path, PFS_DATA_PATH);

  pfs->sml_path = (char *) malloc (strlen (root_path) + 
				   strlen (PFS_SML_PATH) + 1);
  sprintf (pfs->sml_path, "%s%s", root_path, PFS_SML_PATH);

  info_path = (char *) malloc (strlen (root_path) + 
			       strlen (PFS_INFO_PATH) + 1);
  sprintf (info_path, "%s%s", root_path, PFS_INFO_PATH);

  /* Creating sml_file. */
  ASSERT ((fd = open (pfs->sml_path, O_CREAT|O_TRUNC)) >= 0);
  fchmod (fd, S_IRUSR | S_IWUSR);
  close (fd);

  /* Setting up info */
  strncpy (pfs->sd_owner, sd_owner, PFS_NAME_LEN);
  strncpy (pfs->sd_name, sd_name, PFS_NAME_LEN);

  pfs->uid_cnt = 0;
  snprintf (data, 2 * PFS_NAME_LEN + 2, "%s%s%d", 
	    pfs->sd_owner, pfs->sd_name, pfs->uid_cnt);
  mk_hash (data, strlen (data), pfs->sd_id);  
  
  ASSERT ((fd = open (info_path, O_WRONLY|O_TRUNC|O_APPEND|O_CREAT)) >= 0);
  writen (fd, pfs->sd_owner, PFS_NAME_LEN);
  writen (fd, pfs->sd_id, PFS_ID_LEN);
  writen (fd, pfs->sd_name, PFS_NAME_LEN);
  pfs->uid_cnt ++;
  writen (fd, &pfs->uid_cnt, sizeof (uint32_t));
  pfs->uid_cnt --;
  fchmod (fd, S_IRUSR | S_IWUSR);
  close (fd);

  ASSERT (mkdir (pfs->data_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
  
  /* Creating data subdir. */
  data_subdir = (char *) malloc (strlen (pfs->root_path) + 
				 strlen (PFS_DATA_PATH) + 5);
  for (i = 48; i <= 57; i ++)
    {
      sprintf (data_subdir, "%s%s%c/", pfs->root_path, PFS_DATA_PATH, (char) i);
      ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      for (j = 48; j <= 57; j ++) {
	sprintf (data_subdir, "%s%s%c/%c/", pfs->root_path, PFS_DATA_PATH, (char) i, (char) j);
	ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      }      
      for (j = 97; j <= 122; j ++) {
	sprintf (data_subdir, "%s%s%c/%c/", pfs->root_path, PFS_DATA_PATH, (char) i, (char) j);
	ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      }
    }
  
  for (i = 97; i <= 122; i ++)
    {
      sprintf (data_subdir, "%s%s%c/", pfs->root_path, PFS_DATA_PATH, (char) i);
      ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      for (j = 48; j <= 57; j ++) {
	sprintf (data_subdir, "%s%s%c/%c/", pfs->root_path, PFS_DATA_PATH, (char) i, (char) j);
	ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      }      
      for (j = 97; j <= 122; j ++) {
	sprintf (data_subdir, "%s%s%c/%c/", pfs->root_path, PFS_DATA_PATH, (char) i, (char) j);
	ASSERT (mkdir (data_subdir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
      }
    }

  /* Setting up initial group me. */
  pfs->open_file = NULL;
  pfs->grp_cnt = 1;
  pfs->group = (struct pfs_info_group *) malloc (sizeof (struct pfs_info_group));
  
  snprintf (data, PFS_NAME_LEN + 1, "%s", pfs->sd_owner);
  mk_hash (data, strlen (data), pfs->group->id);
  strncpy (pfs->group->name, "me", PFS_NAME_LEN);

  pfs->group->sd_cnt = 1;
  pfs->group->next = NULL;

  pfs->group->sd = (struct pfs_info_sd *) malloc (sizeof (struct pfs_info_sd));
  strncpy (pfs->group->sd->sd_owner, pfs->sd_owner, PFS_NAME_LEN);
  strncpy (pfs->group->sd->sd_name, pfs->sd_name, PFS_NAME_LEN);
  strncpy (pfs->group->sd->sd_id, pfs->sd_id, PFS_ID_LEN);
  pfs->group->sd->next = NULL;

  pfs_init_dir_cache (pfs);

  ASSERT (pfs_create_dir (pfs, pfs->group->v_sd_id, 1) == 0);
  ASSERT (pfs_write_group_info (pfs) == 0);

  free (data_subdir);
  free (pfs->sml_path);
  free (pfs->root_path);
  free (pfs->data_path);
  free (info_path);
  free (pfs->group->sd);
  free (pfs->group);
  free (pfs);

  return 0;
}


/*---------------------------------------------------------------------
 * Method: mk_hash
 * Scope:  Static
 *
 * Make a cryptographic hash of the string
 *
 *---------------------------------------------------------------------*/

void
mk_hash (char * from, size_t len, char * to)
{
  MD5_CTX c;
  MD5_Init(&c);  
  MD5_Update(&c,from,len);
  unsigned char ph[MD5_DIGEST_LENGTH];
  MD5_Final(ph,&c);
  
  char buf[3];
  for(int i=0; i<MD5_DIGEST_LENGTH; i++) {
    sprintf(buf,"%x",ph[i]);
    if(buf[1]=='\0') {
      to[2*i]='0';
      to[2*i+1]=buf[0];
    }
    else {
      to[2*i]=buf[0];
      to[2*i+1]=buf[1];
    }
  }
}
