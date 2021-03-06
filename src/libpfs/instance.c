/*
 * pFS Instance Management
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>

#include "instance.h"
#include "entry.h"
#include "group.h"
#include "lib/sha1.h"

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
  char * buf;

  pfs = (struct pfs_instance *) malloc (sizeof (struct pfs_instance));
  umask (S_IWGRP | S_IWOTH);
  pfs_mutex_init (&pfs->open_lock);
  pfs_mutex_init (&pfs->info_lock);
  pfs_mutex_init (&pfs->group_lock);
  
  /* Set up the paths. */
  if (root_path[strlen (root_path) - 1] != '/') 
    {
      pfs->root_path = (char *) malloc (strlen (root_path) + 2);
      strncpy (pfs->root_path, root_path, strlen (root_path) + 1);
      pfs->root_path[strlen (root_path)] = '/';
      pfs->root_path[strlen (root_path) + 1] = 0;
    }
  else 
    {
      pfs->root_path = (char *) malloc (strlen (root_path) + 1);
      strncpy (pfs->root_path, root_path, strlen (root_path) + 1);
    }
  pfs->data_path = (char *) malloc ((strlen (pfs->root_path) + 
				     strlen (PFS_DATA_PATH) + 1));
  sprintf (pfs->data_path, "%s%s", pfs->root_path, PFS_DATA_PATH);
  info_path = (char *) malloc ((strlen (pfs->root_path) + strlen (PFS_INFO_PATH) + 1));
  sprintf (info_path, "%s%s", pfs->root_path, PFS_INFO_PATH);

  /* Reading info. */
  ASSERT ((fd = open (info_path, O_RDONLY)) >= 0);
  free (info_path);

  buf = readline (fd);
  memset (pfs->sd_owner, 0, PFS_NAME_LEN);
  strcpy (pfs->sd_owner, buf);
  free (buf);
  
  buf = readline (fd);
  strncpy (pfs->sd_id, buf, PFS_ID_LEN);
  free (buf);

  buf = readline (fd);
  memset (pfs->sd_name, 0, PFS_NAME_LEN);
  strcpy (pfs->sd_name, buf);
  free (buf);

  buf = readline (fd);
  pfs->uid_cnt = atoi (buf);
  free (buf);

  buf = readline (fd);
  pfs->updt_cnt = atoi (buf);
  free (buf);

  pfs->info_dirty = 0;

  close (fd);

  /* we use info file as lock file for group open_file list access. */
  pfs->open_file = NULL;
  pfs->group = NULL;
  
  pfs_init_dir_cache (pfs);
  pfs->updt_cb = NULL;

  pfs_group_read (pfs);

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
  /* We close all the files. */
  while (pfs->open_file != NULL) {
    pfs_close (pfs, pfs->open_file->fd);
  }

  pfs_destroy_dir_cache (pfs);
  pfs_write_back_info (pfs);
  pfs_write_back_group (pfs);
  pfs_group_free (pfs);

  pfs_mutex_destroy (&pfs->open_lock);
  pfs_mutex_destroy (&pfs->info_lock);
  pfs_mutex_destroy (&pfs->group_lock);

  free (pfs->root_path);
  free (pfs->data_path);
  free (pfs);

  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_write_back_info
 * Scope:  Global
 *
 * Write back uid_cnt
 *
 *---------------------------------------------------------------------*/

int pfs_write_back_info (struct pfs_instance * pfs)
{
  char * info_path;
  char buf[64];
  int fd;

  pfs_mutex_lock (&pfs->info_lock);
  if (pfs->info_dirty == 0) {
    pfs_mutex_unlock (&pfs->info_lock);
    return 0;
  }
  pfs_mutex_unlock (&pfs->info_lock);

  info_path = (char *) malloc (strlen (pfs->root_path) + 
			       strlen (PFS_INFO_PATH) + 1);
  sprintf (info_path, "%s%s", pfs->root_path, PFS_INFO_PATH);

  if ((fd = open (info_path, O_WRONLY|O_TRUNC|O_APPEND)) < 0) {
    free (info_path);
    return -1;
  }
  free (info_path);

  pfs_mutex_lock (&pfs->info_lock);
  writeline (fd, pfs->sd_owner, strlen (pfs->sd_owner));
  writeline (fd, pfs->sd_id, PFS_ID_LEN);
  writeline (fd, pfs->sd_name, strlen (pfs->sd_name));
  sprintf (buf, "%d", (int) pfs->uid_cnt);
  writeline (fd, buf, strlen (buf));
  sprintf (buf, "%d", (int) pfs->updt_cnt);
  writeline (fd, buf, strlen (buf));
  pfs->info_dirty = 0;
  pfs_mutex_unlock (&pfs->info_lock);

  close (fd);

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

  pfs_mutex_lock (&pfs->info_lock);
  pfs->uid_cnt++;
  pfs->info_dirty = 1;
  memcpy (data, pfs->sd_id, PFS_ID_LEN);
  sprintf (data + PFS_ID_LEN, "%d", (int) pfs->uid_cnt);
  pfs_mutex_unlock (&pfs->info_lock);
  mk_hash (data, strlen (data), id);
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_incr_updt_cnt
 * Scope:  Global
 *
 * Increase and returns updt_cnt
 *
 *---------------------------------------------------------------------*/

uint64_t
pfs_incr_updt_cnt (struct pfs_instance * pfs)
{
  int retval;
  pfs_mutex_lock (&pfs->info_lock);
  pfs->updt_cnt ++;
  pfs->info_dirty = 1;
  retval = pfs->updt_cnt;
  pfs_mutex_unlock (&pfs->info_lock);
  return retval;
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
  char * group_path;
  int fd, i, j;
  char * data_subdir;

  pfs = (struct pfs_instance *) malloc (sizeof (struct pfs_instance));
  umask (S_IWGRP | S_IWOTH);
  pfs_mutex_init (&pfs->open_lock);
  pfs_mutex_init (&pfs->info_lock);
  pfs_mutex_init (&pfs->group_lock);

  /* Setting up paths. */

  if (root_path[strlen (root_path) - 1] != '/') 
    {
      pfs->root_path = (char *) malloc (strlen (root_path) + 2);
      strncpy (pfs->root_path, root_path, strlen (root_path) + 1);
      pfs->root_path[strlen (root_path)] = '/';
      pfs->root_path[strlen (root_path) + 1] = 0;
    }
  else 
    {
      pfs->root_path = (char *) malloc (strlen (root_path) + 1);
      strncpy (pfs->root_path, root_path, strlen (root_path) + 1);
    }

  printf ("root_path : %s\n", pfs->root_path);
  pfs->data_path = (char *) malloc (strlen (pfs->root_path) + 
				    strlen (PFS_DATA_PATH) + 1);
  sprintf (pfs->data_path, "%s%s", pfs->root_path, PFS_DATA_PATH);
  info_path = (char *) malloc (strlen (pfs->root_path) + 
			       strlen (PFS_INFO_PATH) + 1);
  sprintf (info_path, "%s%s", pfs->root_path, PFS_INFO_PATH);
  group_path = (char *) malloc (strlen (pfs->root_path) + 
			       strlen (PFS_GROUP_PATH) + 1);
  sprintf (group_path, "%s%s", pfs->root_path, PFS_GROUP_PATH);

  /* Setting up info */
  strncpy (pfs->sd_owner, sd_owner, PFS_NAME_LEN);
  strncpy (pfs->sd_name, sd_name, PFS_NAME_LEN);
  pfs->uid_cnt = 0;
  pfs->updt_cnt = 0;
  snprintf (data, 2 * PFS_NAME_LEN + 2, "%s%s%d", 
	    pfs->sd_owner, pfs->sd_name, (int) pfs->uid_cnt);
  mk_hash (data, strlen (data), pfs->sd_id); 
  pfs->info_dirty = 1;

  ASSERT ((fd = open (info_path, O_WRONLY|O_TRUNC|O_APPEND|O_CREAT)) >= 0);
  fchmod (fd, S_IRUSR | S_IWUSR);
  close (fd);

  /* Setting up group file. */
  ASSERT ((fd = open (group_path, O_WRONLY|O_TRUNC|O_APPEND|O_CREAT)) >= 0);
  fchmod (fd, S_IRUSR | S_IWUSR);
  close (fd);  
  
  /* Creating data subdir. */
  ASSERT (mkdir (pfs->data_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0);
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

  /* Setting up group info. */
  pfs->grp_cnt = 0;
  pfs->group = NULL;
  pfs->grp_dirty = 1;
  
  /* Writing back info. */
  pfs_write_back_info (pfs);
  pfs_write_back_group (pfs);

  pfs_mutex_destroy (&pfs->open_lock);
  pfs_mutex_destroy (&pfs->info_lock);
  pfs_mutex_destroy (&pfs->group_lock);

  /* Cleaning. */
  free (data_subdir);
  free (pfs->root_path);
  free (pfs->data_path);
  free (info_path);
  free (group_path);
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

static void
mk_hash (char * from, size_t len, char * to)
{
  int i;
  unsigned char ht [20];
  unsigned int val;
  static char hex[] = "0123456789abcdef";
  SHA_CTX c;

  SHA1_Init (&c);
  SHA1_Update (&c, from, len);
  SHA1_Final (ht, &c);

  for (i = 0; i < 20; i ++) {
    val = ht[i];
    *to++ = hex[val >> 4];
    *to++ = hex[val & 0xf];
  }
}
