/*
 * Update callback
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_UPDT_H
#define _PFS_UPDT_H

#include "instance.h"

struct pfs_updt
{
  char grp_id [PFS_ID_LEN];
  char dir_id [PFS_ID_LEN];  
  char name [PFS_NAME_LEN];
  uint8_t reclaim;
  struct pfs_ver * ver;
};


int pfs_push_log_entry (struct pfs_instance * pfs,
			const char * grp_id,
			const char * dir_id,
			const char * name,
			const uint8_t reclaim,
			const struct pfs_ver * ver);

void pfs_write_updt (int fd,
		     const struct pfs_updt * updt);

struct pfs_updt * pfs_read_updt (int fd);
void pfs_free_updt (struct pfs_updt * updt);
struct pfs_updt * pfs_cpy_updt (const struct pfs_updt * updt);
void pfs_print_updt (struct pfs_updt * updt);

#endif /* _PFS_UPDT_H */

