/*
 * Group related functions
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_GROUP_H
#define _PFS_GROUP_H

#include "instance.h"

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


int pfs_group_read (struct pfs_instance * pfs);
int pfs_group_free (struct pfs_instance * pfs);



#endif
