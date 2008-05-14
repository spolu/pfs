/*
 * Path -> id resolution
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>

#include "path.h"
#include "instance.h"
#include "entry.h"
#include "group.h"

static int pfs_split_name (struct pfs_instance * pfs,
			   char * name,
			   char * file_name,
			   uint8_t * is_main,
			   char * sd_owner,
			   char * sd_name);

/*---------------------------------------------------------------------
 * Method: pfs_get_path_info
 * Scope: Global
 *
 * Parse path and set path_info
 * If this is a group directory
 *---------------------------------------------------------------------*/

int pfs_get_path_info (struct pfs_instance * pfs,
		       const char * path,
		       struct pfs_path_info * pi)
{
  int i, retval;
  char * p = NULL;
  char * token, * brk;
  char * sep = "/";
  int cnt = 0;
  char sd_name [PFS_NAME_LEN];
  char sd_owner [PFS_NAME_LEN];
  struct pfs_entry * entry;
  struct pfs_ver * ver;

  retval = -ENOENT;

  if (path[0] != '/' || path[strlen (path) - 1] == '/') {
    retval = -EACCES;
    goto error;
  }
  
  p = (char *) malloc (strlen (path));
  if (p == NULL) {
    retval = -EIO;
    goto error;
  }
  strcpy (p, path + 1);
  
  for (token = strtok_r (p, sep, &brk);
       token;
       token = strtok_r (NULL, sep, &brk))
    {

      if (strlen (token) > PFS_NAME_LEN - 1) {
        retval = -ENAMETOOLONG;
	goto error;
      }

      if (cnt == 0) {
	if (pfs_get_grp_id (pfs, token, pi->grp_id) != 0)
	  goto error;
	strncpy (pi->dir_id, pi->grp_id, PFS_ID_LEN);
	pi->is_main = 0;
	strncpy (pi->name, token, PFS_NAME_LEN);
	strncpy (pi->dst_id, pi->dir_id, PFS_ID_LEN);
	pi->type = PFS_GRP;
	pi->st_mode = S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP 
	  | S_IXGRP | S_IROTH | S_IXGRP | S_IFDIR;
	cnt ++;
      }

      else {
	strncpy (pi->dir_id, pi->dst_id, PFS_ID_LEN);

	if (pfs_split_name (pfs, token, pi->name, &pi->is_main, sd_owner, sd_name) != 0)
	  goto error;
	if (pi->is_main == 0)
	  pfs_get_sd_id (pfs, pi->grp_id, sd_owner, sd_name, pi->last_updt);

	entry = pfs_get_entry (pfs, pi->dir_id, pi->name);
	if (entry == NULL)
	  goto error;

	ver = NULL;
	if (pi->is_main == 1) {
	  ver = entry->ver[entry->main_idx];
	  strncpy (pi->last_updt, ver->vv->last_updt, PFS_ID_LEN);
	}
	else {
	  for (i = 0; i < entry->ver_cnt; i ++) {
	    if (memcmp (pi->last_updt, entry->ver[i]->vv->last_updt, PFS_ID_LEN) == 0)
	      ver = entry->ver[i];
	  }
	}
	if (ver == NULL) {
	  pfs_free_entry (entry);
	  goto error;
	}

	strncpy (pi->dst_id, ver->dst_id, PFS_ID_LEN);
	pi->type = ver->type;
	pi->st_mode = ver->st_mode;
	pfs_free_entry (entry);
      }

    }  
  
  free (p);
  return 0;

 error:
  if (p != NULL)
    free (p);
  return retval;
}

/*---------------------------------------------------------------------
 * Method: pfs_split_name
 * Scope: Static
 *
 * Extract real file name from name.
 *
 *---------------------------------------------------------------------*/

static int pfs_split_name (struct pfs_instance * pfs,
			   char * name,
			   char * file_name,
			   uint8_t * is_main,
			   char * sd_owner,
			   char * sd_name)
{
  char * token;

  token = strstr (name, ":");
  if (token == NULL) {
    if (strlen (name) > PFS_NAME_LEN)
      return -1;
    strncpy (file_name, name, PFS_NAME_LEN);
    *is_main = 1;
    return 0;
  }

  *token = 0;
  token += 1;

  strncpy (file_name, token, PFS_NAME_LEN);
  *is_main = 0;
  
  token = strstr (name, ".");
  if (token == NULL)
    return -1;
  
  *token = 0;
  token += 1;
  
  strncpy (sd_owner, name, PFS_NAME_LEN);
  strncpy (sd_name, token, PFS_NAME_LEN);

  return 0;  
}

