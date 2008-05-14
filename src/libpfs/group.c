/*
 * Group related functions
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
#include <openssl/md5.h>
#include <sys/file.h>

#include "instance.h"
#include "group.h"
#include "path.h"

/*---------------------------------------------------------------------
 * Method: pfs_get_grp_id
 * Scope: Global
 *
 * get the grp_id from grp_name
 *
 *---------------------------------------------------------------------*/

int pfs_get_grp_id (struct pfs_instance * pfs,
		     const char * grp_name,
		     char * grp_id)
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
      return 0;
    }
    next_grp = next_grp->next;
  }
  
  if (redo == 1)
    return -1;

  /* We update the group_info mayber a user/group has been created. */
  pfs_group_free (pfs);
  pfs_group_read (pfs);
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

  if (redo == 1) {
    ASSERT (0);
    return -1;
  }

  /* We update the group_info mayber a user/group has been created. */
  pfs_group_free (pfs);
  pfs_group_read (pfs);
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

  if (redo == 1) {
    sprintf (sd_owner, "UNRESOLVED");
    sprintf (sd_name, "%.*s", PFS_ID_LEN, sd_id); 
    return 0;
  }

  /* We update the group_info mayber a user/group has been created. */
  pfs_group_free (pfs);
  pfs_group_read (pfs);
  redo = 1;

  goto redo;

  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_(read|free|write)_group_info
 * Scope:  Global
 *
 * Load or free info field of pfs instance. Used to initially load info
 * Format .pfs/group : name:id
 * Format /group/.pfs : id:owner:name
 *
 *---------------------------------------------------------------------*/

int
pfs_group_read (struct pfs_instance * pfs)
{
  int fd;
  int i,j;
  struct pfs_info_group * new_grp;
  struct pfs_info_sd * new_sd;
  char * group_path;  
  char * buf;
  struct pfs_path_info pi;

  ASSERT (pfs->group == NULL);
  group_path = (char *) malloc (strlen (pfs->root_path) + 
				strlen (PFS_GROUP_PATH) + 1);
  sprintf (group_path, "%s%s", pfs->root_path, PFS_GROUP_PATH);
  ASSERT ((fd = open (group_path, O_RDONLY)) > 0);
  flock (fd, LOCK_SH);

  while ((buf = readline (fd)) != NULL && strlen (buf) > 0)
    {
      new_grp = (struct pfs_info_group *) malloc (sizeof (struct pfs_info_group));
      for (i = 0; buf[i] != 0 && buf[i] != ':'; i ++);
      ASSERT (buf[i] != 0 && i != 0);
      buf[i] = 0;

      memset (new_grp->name, 0, PFS_NAME_LEN);
      strcpy (new_grp->name, buf);

      ASSERT (strlen (buf + (i+1)) == PFS_ID_LEN);
      strncpy (new_grp->id, buf + (i+1), PFS_ID_LEN);

      free (buf);
      new_grp->sd = NULL;

      printf ("found group : %s:%.*s\n", new_grp->name, PFS_ID_LEN, new_grp->id);

      new_grp->next = pfs->group;
      pfs->group = new_grp;
    }

  free (group_path);
  flock (fd, LOCK_UN);
  close (fd);
  
  new_grp = pfs->group;
  while (new_grp != NULL)
    {
      group_path = (char *) malloc (strlen (new_grp->name) + 8);
      sprintf (group_path, "/%s/.pfs", new_grp->name);
      
      if (pfs_get_path_info (pfs, group_path, &pi) != 0) {
	free (group_path);
	new_grp = new_grp->next;
	continue;
      }

      free (group_path);
      group_path = pfs_mk_file_path (pfs, pi.dst_id);
      
      ASSERT ((fd = open (group_path, O_RDONLY)) > 0);
      free (group_path);
      ASSERT (new_grp->sd == NULL);

      while ((buf = readline (fd)) != NULL && strlen (buf) > 0)
	{
	  new_sd = (struct pfs_info_sd *) malloc (sizeof (struct pfs_info_sd));
	  
	  for (i = 0; buf[i] != 0 && buf[i] != ':'; i ++);
	  ASSERT (buf[i] != 0 && i != 0);
	  buf[i] = 0;	  
	  
	  ASSERT (strlen (buf) == PFS_ID_LEN);
	  strncpy (new_sd->sd_id, buf, PFS_ID_LEN);

	  i += 1;
	  for (j = i; buf[j] != 0 && buf[j] != ':'; j ++);
	  ASSERT (buf[j] != 0 && j != 0);
	  buf[j] = 0;

	  memset (new_sd->sd_owner, 0, PFS_NAME_LEN);
	  strcpy (new_sd->sd_owner, buf + i);
	  
	  j += 1;
	  memset (new_sd->sd_name, 0, PFS_NAME_LEN);
	  strcpy (new_sd->sd_name, buf + j);
	  
	  free (buf);
	  
	  printf ("found sd : %.*s:%s:%s\n", PFS_ID_LEN, new_sd->sd_id, new_sd->sd_owner, new_sd->sd_name);

	  new_sd->next = new_grp->sd;
	  new_grp->sd = new_sd;
	}

      close (fd);
      new_grp = new_grp->next;
    }

  return 0;
}


int
pfs_group_free (struct pfs_instance * pfs)
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




