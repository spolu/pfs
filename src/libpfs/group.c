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
  struct pfs_group * next_grp;
  
  if (pfs->group == NULL)
    return -1;

  pfs_mutex_lock (&pfs->group_lock);
  next_grp = pfs->group;
  
  while (next_grp != NULL) {
    if (strncmp (grp_name, next_grp->grp_name, PFS_NAME_LEN) == 0) {
      memcpy (grp_id, next_grp->grp_id, PFS_ID_LEN);
      pfs_mutex_unlock (&pfs->group_lock);
      return 0;
    }
    next_grp = next_grp->next;
  }
  pfs_mutex_unlock (&pfs->group_lock);
  
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
  struct pfs_group * next_grp;
  struct pfs_sd * next_sd;

  if (pfs->group == NULL)
    return -1;

  pfs_mutex_lock (&pfs->group_lock);
  next_grp = pfs->group;

  while (next_grp != NULL) {
    if (strncmp (grp_id, next_grp->grp_id, PFS_ID_LEN) == 0) {
      next_sd = next_grp->sd;
      while (next_sd != NULL) {
	if (strncmp (sd_owner, next_sd->sd_owner, PFS_NAME_LEN) == 0 &&
	    strncmp (sd_name, next_sd->sd_name, PFS_NAME_LEN) == 0) {
	  memcpy (sd_id, next_sd->sd_id, PFS_ID_LEN);
	  pfs_mutex_unlock (&pfs->group_lock);
	  return 0;
	}
	next_sd = next_sd->next;
      }
    }
    next_grp = next_grp->next;
  }
  pfs_mutex_unlock (&pfs->group_lock);

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
  struct pfs_group * next_grp;
  struct pfs_sd * next_sd;

  if (pfs->group == NULL)
    return -1;

  pfs_mutex_lock (&pfs->group_lock);
  next_grp = pfs->group;
  
  while (next_grp != NULL) {
    if (strncmp (grp_id, next_grp->grp_id, PFS_ID_LEN) == 0) {
      next_sd = next_grp->sd;
      while (next_sd != NULL) {
	if (strncmp (sd_id, next_sd->sd_id, PFS_ID_LEN) == 0) {
	  strncpy (sd_owner, next_sd->sd_owner, PFS_NAME_LEN);
	  strncpy (sd_name, next_sd->sd_name, PFS_NAME_LEN);
	  pfs_mutex_unlock (&pfs->group_lock);
	  return 0;
	}
	next_sd = next_sd->next;
      }
    }
    next_grp = next_grp->next;
  }
  pfs_mutex_unlock (&pfs->group_lock);

  return -1;
}


int
pfs_group_updt_sv (struct pfs_instance * pfs,
		   const char * grp_id,
		   const struct pfs_vv * mv)
{
  struct pfs_group * next_grp;
  struct pfs_sd * next_sd;
  struct pfs_vv * sv;

  if (pfs->group == NULL)
    return -1;

  pfs_mutex_lock (&pfs->group_lock);
  next_grp = pfs->group;
  
  while (next_grp != NULL) {
    if (strncmp (grp_id, next_grp->grp_id, PFS_ID_LEN) == 0) 
      {
	if ((sv = pfs_vv_merge (pfs, mv, next_grp->grp_sv)) == NULL) {
	  pfs_mutex_unlock (&pfs->group_lock);
	  return -1;
	}
	pfs_free_vv (next_grp->grp_sv);
	next_grp->grp_sv = sv;
	
	next_sd = next_grp->sd;	
	while (next_sd != NULL)
	  {
	    if (strncmp (pfs->sd_id, next_sd->sd_id, PFS_ID_LEN) == 0) {
	      pfs_free_vv (next_sd->sd_sv);
	      next_sd->sd_sv = pfs_cpy_vv (next_grp->grp_sv);
	      pfs_mutex_unlock (&pfs->group_lock);
	      return 0;
	    }
	  }
	
	pfs_mutex_unlock (&pfs->group_lock);
	return -1;
      }
    next_grp = next_grp->next;
  }
  pfs_mutex_unlock (&pfs->group_lock);

  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_group_add
 * Scope:  Global
 *
 *
 *---------------------------------------------------------------------*/

int
pfs_group_add (struct pfs_instance * pfs,
	       const char * grp_name,
	       const char * grp_id)
{
  struct pfs_group * new_grp;

  new_grp = (struct pfs_group *) malloc (sizeof (struct pfs_group));
  strncpy (new_grp->grp_name, grp_name, PFS_NAME_LEN);
  strncpy (new_grp->grp_id, grp_id, PFS_ID_LEN);

  /* grp_sv init. */
  new_grp->grp_sv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  new_grp->grp_sv->len = 1;

  new_grp->grp_sv->sd_id = (char **) malloc (sizeof (char *));
  new_grp->grp_sv->sd_id[0] = (char *) malloc (PFS_ID_LEN);
  strncpy (new_grp->grp_sv->sd_id[0], pfs->sd_id, PFS_ID_LEN);

  new_grp->grp_sv->value = (uint64_t *) malloc (sizeof (uint64_t));
  pfs_mutex_lock (&pfs->info_lock);
  pfs->updt_cnt ++;
  new_grp->grp_sv->value[0] = pfs->updt_cnt;
  pfs_mutex_unlock (&pfs->info_lock);

  /* sd init. */
  new_grp->sd = (struct pfs_sd *) malloc (sizeof (struct pfs_sd));
  strncpy (new_grp->sd->sd_id, pfs->sd_id, PFS_ID_LEN);
  strncpy (new_grp->sd->sd_name, pfs->sd_name, PFS_NAME_LEN);
  strncpy (new_grp->sd->sd_owner, pfs->sd_owner, PFS_NAME_LEN);
  new_grp->sd->sd_sv = pfs_cpy_vv (new_grp->grp_sv);
  new_grp->sd->next = NULL;

  new_grp->sd_cnt = 1;

  pfs_mutex_lock (&pfs->group_lock);
  new_grp->next = pfs->group;
  pfs->group = new_grp;
  pfs->grp_cnt += 1;
  pfs_mutex_unlock (&pfs->group_lock);
  
  return 0;
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
  struct pfs_group * new_grp;
  struct pfs_sd * new_sd;
  char * group_path;  

  pfs_mutex_lock (&pfs->group_lock);

  ASSERT (pfs->group == NULL);
  group_path = (char *) malloc (strlen (pfs->root_path) + 
				strlen (PFS_GROUP_PATH) + 1);
  sprintf (group_path, "%s%s", pfs->root_path, PFS_GROUP_PATH);
  ASSERT ((fd = open (group_path, O_RDONLY)) > 0);
  free (group_path);
  flock (fd, LOCK_SH);

  ASSERT (pfs->group == NULL);

  readn (fd, &pfs->grp_cnt, sizeof (uint32_t));

  for (i = 0; i < pfs->grp_cnt; i ++)
    {      
      new_grp = (struct pfs_group *) malloc (sizeof (struct pfs_group));

      readn (fd, new_grp->grp_id, PFS_ID_LEN);
      readn (fd, new_grp->grp_name, PFS_NAME_LEN);
      new_grp->grp_sv = pfs_read_vv (fd);
      readn (fd, &new_grp->sd_cnt, sizeof (uint32_t));

      new_grp->sd = NULL;
      printf ("found group : %s:%.*s\n", new_grp->grp_name, PFS_ID_LEN, new_grp->grp_id);

      for (j = 0; j < new_grp->sd_cnt; j ++)
	{
	  new_sd = (struct pfs_sd *) malloc (sizeof (struct pfs_sd));
	  
	  readn (fd, new_sd->sd_id, PFS_ID_LEN);
	  readn (fd, new_sd->sd_owner, PFS_NAME_LEN);
	  readn (fd, new_sd->sd_name, PFS_NAME_LEN);
	  new_sd->sd_sv = pfs_read_vv (fd);

	  printf ("found sd : %.*s:%s:%s\n", PFS_ID_LEN, new_sd->sd_id, new_sd->sd_owner, new_sd->sd_name);

	  new_sd->next = new_grp->sd;
	  new_grp->sd = new_sd;
	}

      new_grp->next = pfs->group;
      pfs->group = new_grp;
    }
  
  flock (fd, LOCK_UN);
  close (fd);
  
  pfs_mutex_unlock (&pfs->group_lock);
  return 0;
}


int
pfs_group_free (struct pfs_instance * pfs)
{
  struct pfs_group * next_group;
  struct pfs_sd * next_sd;

  pfs_mutex_lock (&pfs->group_lock);
  while (pfs->group != NULL) {
    next_group = pfs->group->next;
    pfs_free_vv (pfs->group->grp_sv);
    while (pfs->group->sd != NULL) {
      pfs_free_vv (pfs->group->sd->sd_sv);
      next_sd = pfs->group->sd->next;
      free (pfs->group->sd);
      pfs->group->sd = next_sd;
    }
    free (pfs->group);
    pfs->group = next_group;
  }
  pfs->group = NULL;
  pfs_mutex_unlock (&pfs->group_lock);

  return 0;
}


int
pfs_write_back_group (struct pfs_instance * pfs)
{
  int fd;
  int i,j;
  struct pfs_group * new_grp;
  struct pfs_sd * new_sd;
  char * group_path;  

  pfs_mutex_lock (&pfs->group_lock);

  group_path = (char *) malloc (strlen (pfs->root_path) + 
				strlen (PFS_GROUP_PATH) + 1);
  sprintf (group_path, "%s%s", pfs->root_path, PFS_GROUP_PATH);
  ASSERT ((fd = open (group_path, O_TRUNC | O_APPEND | O_WRONLY)) > 0);
  free (group_path);
  flock (fd, LOCK_EX);

  new_grp = pfs->group;

  writen (fd, &pfs->grp_cnt, sizeof (uint32_t));

  for (i = 0; i < pfs->grp_cnt; i ++)
    {      
      writen (fd, new_grp->grp_id, PFS_ID_LEN);
      writen (fd, new_grp->grp_name, PFS_NAME_LEN);
      pfs_write_vv (fd, new_grp->grp_sv);
      writen (fd, &new_grp->sd_cnt, sizeof (uint32_t));

      printf ("wrote group : %s:%.*s\n", new_grp->grp_name, PFS_ID_LEN, new_grp->grp_id);
      
      new_sd = new_grp->sd;

      printf ("new_grp sd_cnt : %d\n", new_grp->sd_cnt);

      for (j = 0; j < new_grp->sd_cnt; j ++)
	{	  
	  writen (fd, new_sd->sd_id, PFS_ID_LEN);
	  writen (fd, new_sd->sd_owner, PFS_NAME_LEN);
	  writen (fd, new_sd->sd_name, PFS_NAME_LEN);
	  pfs_write_vv (fd, new_sd->sd_sv);

	  printf ("wrote sd : %.*s:%s:%s\n", PFS_ID_LEN, new_sd->sd_id, new_sd->sd_owner, new_sd->sd_name);

	  new_sd = new_sd->next;
	}

      new_grp = new_grp->next;
    }
  
  flock (fd, LOCK_UN);
  close (fd);
  
  pfs_mutex_unlock (&pfs->group_lock);

  printf ("Wrote back group\n");
  return 0;  
}

