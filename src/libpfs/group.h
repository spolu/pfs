/*
 * Group related functions
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_GROUP_H
#define _PFS_GROUP_H

#include "instance.h"

#define PFS_GROUP_PATH "groups"

struct pfs_group
{
  char grp_id [PFS_ID_LEN];      
  char grp_name [PFS_NAME_LEN];  

  struct pfs_vv * grp_sv;

  uint32_t sd_cnt;
  struct pfs_sd * sd;
  struct pfs_group * next;
};


struct pfs_sd
{
  char sd_id [PFS_ID_LEN];
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];
  struct pfs_vv * sd_sv;

  struct pfs_sd * next;
};





int pfs_get_grp_id (struct pfs_instance * pfs,
		    const char * grp_name,
		    char * grp_id);
int pfs_get_sd_id (struct pfs_instance * pfs,
		   const char * grp_id,
		   const char * sd_owner,
		   const char * sd_name,
		   char * sd_id);
int pfs_get_sd_info (struct pfs_instance * pfs,
		     const char * grp_id,
		     const char * sd_id,
		     char * sd_owner,
		     char * sd_name);

int pfs_group_updt_sv (struct pfs_instance * pfs,
		       const char * grp_id,
		       const struct pfs_vv * mv);

int pfs_group_add (struct pfs_instance * pfs,
		   const char * grp_name,
		   const char * grp_id);

int pfs_group_read (struct pfs_instance * pfs);
int pfs_group_free (struct pfs_instance * pfs);
int pfs_write_back_group (struct pfs_instance * pfs);



#endif
