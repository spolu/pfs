/*
 * File Helper functions
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>

#include "file.h"


/*---------------------------------------------------------------------
 * Method: pfs_file_link_new_id
 * Scope:  Global
 *
 * Links the file represented by file_id to a new id returned. NULL if
 * an error is encountered.
 *
 *---------------------------------------------------------------------*/

int
pfs_file_link_new_id (struct pfs_instance * pfs,
		      const char * file_id,
		      char * new_id)
{
  char * file_path;
  char * new_file_path;

  pfs_mk_id (pfs, new_id);

  file_path = pfs_mk_file_path (pfs, file_id);
  new_file_path = pfs_mk_file_path (pfs, new_id);
    
  if (link (file_path, new_file_path) != 0) {
    free (file_path);
    free (new_file_path);
    return -errno;
  }
  
  free (file_path);
  free (new_file_path);
  return 0;
}

/*---------------------------------------------------------------------
 * Method: pfs_file_unlink
 * Scope:  Global
 *
 * unlink the file denoted by file_id.
 *
 *---------------------------------------------------------------------*/

int
pfs_file_unlink (struct pfs_instance * pfs,
		 char * file_id)
{
  char * file_path;
  file_path = pfs_mk_file_path (pfs, file_id);
  if (unlink (file_path) != 0) {
    free (file_path);
    return -errno;
  }
  free (file_path);
  return 0;
}

/*---------------------------------------------------------------------
 * Method: pfs_file_unlink
 * Scope:  Global
 *
 * Creates a new file and set id to the newly generated uid
 * Returns the new file descriptor
 *
 *---------------------------------------------------------------------*/

int
pfs_file_create (struct pfs_instance * pfs,
		 char * id,
		 int flags)
{
  char * file_path;
  int fd = -1;
  
  if (!(flags & O_CREAT))
    return -1;

  pfs_mk_id (pfs, id);
  file_path = pfs_mk_file_path (pfs, id);

  if ((fd = open (file_path, 
		  flags, 
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
    free (file_path);
    return -errno;
  }  

  free (file_path);

  return fd;
}

/*---------------------------------------------------------------------
 * Method: pfs_file_copy_id
 * Scope:  Global
 *
 * Copy blob and assign a new id to copy. Used for link semantics.
 * 
 *---------------------------------------------------------------------*/

int
pfs_file_copy_id (struct pfs_instance * pfs,
		  const char * file_id,
		  char * new_id)
{
  char * file_path;
  char * new_file_path;
  int fd_from;
  int fd_to;
  ssize_t len;
  char buf[4096];

  pfs_mk_id (pfs, new_id);

  file_path = pfs_mk_file_path (pfs, file_id);
  new_file_path = pfs_mk_file_path (pfs, new_id);

  if ((fd_from = open (file_path, O_RDONLY)) < 0) {
    free (file_path);
    free (new_file_path);
    return -errno;
  }
    
  if ((fd_to = open (new_file_path, 
		     O_CREAT | O_WRONLY | O_APPEND,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
    close (fd_from);
    free (file_path);
    free (new_file_path);
    return -errno;
  }
  
  while ((len = readn (fd_from, buf, 4096)) > 0) {
    writen (fd_to, buf, len);
  }
  
  close (fd_from);
  close (fd_to);  
  free (file_path);
  free (new_file_path);

  return 0;
}
