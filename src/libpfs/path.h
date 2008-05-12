/*
 * Path -> id resolution
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_PATH_H
#define _PFS_PATH_H

#include "pfs.h"
#include "entry.h"

#include <sys/types.h>
#include <sys/stat.h>

struct pfs_path_info {
  char grp_id [PFS_ID_LEN];
  char dir_id [PFS_ID_LEN];
  char name [PFS_NAME_LEN];
  char dst_id [PFS_ID_LEN];
  uint8_t is_main;
  char last_updt [PFS_ID_LEN];
  uint8_t type;
  mode_t st_mode;
};

int pfs_get_path_info (struct pfs_instance * pfs,
		       const char * path,
		       struct pfs_path_info * pi);


#endif
