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
#define PFS_SML_PATH "sml"

#define PFS_GROUP_INFO_PATH "groups"
#define PFS_INFO_PATH "info"

#define PFS_PUSH_LOG_PATH "push_log"


struct pfs_ver;

struct pfs_instance
{
  char sd_owner [PFS_NAME_LEN];
  char sd_id [PFS_ID_LEN];
  char sd_name [PFS_NAME_LEN];
  uint32_t uid_cnt;

  pdc_t * dir_cache;

  int (*updt_cb)(struct pfs_instance *, struct pfs_updt *);
  
  char * root_path;
  char * data_path;
  char * sml_path;

  struct pfs_mutex open_lock;
  struct pfs_open_file * open_file;

  uint32_t grp_cnt;
  struct pfs_info_group * group;
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



/* INFOS */

struct pfs_info_group
{
  char id [PFS_ID_LEN];        /* Group id.                */
  char name [PFS_NAME_LEN];    /* Group name.              */
  char v_sd_id [PFS_ID_LEN];   /* Local v_sd id.           */

  uint32_t sd_cnt;  
  struct pfs_info_sd * sd;

  struct pfs_info_group * next;
};


struct pfs_info_sd
{
  char sd_id [PFS_ID_LEN];
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];

  struct pfs_info_sd * next;
};




struct pfs_instance * pfs_init_instance (const char * root_path);
int pfs_destroy_instance (struct pfs_instance * pfs);


int pfs_mk_id (struct pfs_instance * pfs, char * id);
char * pfs_mk_file_path (struct pfs_instance * pfs, const char * id);
char * pfs_mk_dir_path (struct pfs_instance * pfs, const char * id);



int pfs_get_v_sd_id (struct pfs_instance * pfs,
		     const char * grp_name,
		     char * grp_id,
		     char * v_sd_id);
int pfs_get_sd_id (struct pfs_instance * pfs,
		   const char * grp_id,
		   const char * sd_owner,
		   const char * sd_name,
		   char * sd_id);
int pfs_get_sd_info (struct pfs_instance * pfs,
		     const char * grp_name,
		     const char * sd_id,
		     char * sd_owner,
		     char * sd_name);


int pfs_read_group_info (struct pfs_instance * pfs);
int pfs_free_group_info (struct pfs_instance * pfs);
int pfs_write_group_info (struct pfs_instance * pfs);



int pfs_bootstrap_inst (const char * root_path,
			const char * sd_owner,
			const char * sd_name);

#endif
