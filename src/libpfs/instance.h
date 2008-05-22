/*
 * pFS Instance Management
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_INSTANCE_H
#define _PFS_INSTANCE_H

#include <stdint.h>

#include "lib/debug.h"
#include "lib/io.h"
#include "lib/lock.h"

#include "pfs.h"
#include "dir_cache.h"

#define PFS_DATA_PATH "data/"
#define PFS_INFO_PATH "info"


struct pfs_ver;

struct pfs_instance
{
  char sd_owner [PFS_NAME_LEN];
  char sd_id [PFS_ID_LEN];
  char sd_name [PFS_NAME_LEN];

  char * root_path;
  char * data_path;

  pdc_t * dir_cache;

  struct pfs_mutex info_lock;
  uint64_t uid_cnt;
  uint64_t updt_cnt;
  int (*updt_cb)(struct pfs_instance *, struct pfs_updt *);
  uint8_t info_dirty;

  struct pfs_mutex open_lock;
  struct pfs_open_file * open_file;

  struct pfs_mutex group_lock;
  uint32_t grp_cnt;
  uint8_t grp_dirty;
  struct pfs_group * group;
};



/* OPEN FILES */

struct pfs_open_file
{
  char id [PFS_ID_LEN];

  int fd;

  uint8_t dirty;
  uint8_t read_only;
  
  char grp_id [PFS_ID_LEN];
  char dir_id [PFS_ID_LEN];
  char file_name [PFS_NAME_LEN];
  struct pfs_ver * ver;
  
  struct pfs_open_file * prev;
  struct pfs_open_file * next;
};



struct pfs_instance * pfs_init_instance (const char * root_path);
int pfs_destroy_instance (struct pfs_instance * pfs);
int pfs_write_back_info (struct pfs_instance * pfs);

int pfs_mk_id (struct pfs_instance * pfs, char * id);
int pfs_incr_updt_cnt (struct pfs_instance * pfs);

char * pfs_mk_file_path (struct pfs_instance * pfs, const char * id);
char * pfs_mk_dir_path (struct pfs_instance * pfs, const char * id);

int pfs_bootstrap_inst (const char * root_path,
			const char * sd_owner,
			const char * sd_name);

#endif
