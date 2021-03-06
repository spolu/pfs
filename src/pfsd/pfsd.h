/*
 * pFS Daemon
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */


#ifndef _PFSD_H
#define _PFSD_H

#include "../libpfs/pfs.h"
#include "global.h"
#include "../libpfs/lib/lock.h"

#define PFSD_LOG_PATH "pfsd_log"
#define WRITE_BACK_SLEEP 20
#define COMMIT_UPDT_SLEEP 2

struct pfsd_state * pfsd;

struct pfsd_state
{
  struct pfs_instance * pfs;
  int tun_pid;
  int pfsd_port;
  

  char * log_path;
  uint8_t update;
  
  struct pfsd_log * log;
  struct pfs_mutex log_lock;

  uint32_t updt_cnt;
  struct pfs_updt * updt;
  struct pfs_mutex updt_lock;

  uint32_t sd_cnt;
  struct pfsd_sd * sd;
  struct pfs_mutex sd_lock;
};


struct pfsd_sd
{
  char sd_id [PFS_ID_LEN];
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];

  uint8_t tun_conn;
  int tun_port;

  struct pfsd_sd * next;
};


void * start_write_back (void * tid);
void * commit_updt (void * tid);

int updt_cb (struct pfs_instance * pfs,
	     struct pfs_updt * updt);

int pfsd_init (struct pfs_instance * pfs, int pfsd_port);
int pfsd_destroy ();

#endif
