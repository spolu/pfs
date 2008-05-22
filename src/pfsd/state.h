/*
 * pFS Daemon : state structure
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFSD_STATE_H
#define _PFSD_STATE_H

#include <stdint.h>
#include <sys/types.h>

#include "../libpfs/updt.h"
#include "../libpfs/lib/lock.h"
#include "../libpfs/lib/io.h"
#include "../libpfs/pfs.h"
#include "../libpfs/updt.h"

#include "log.h"

/* Global variables */

struct pfs_instance * pfs;
struct pfsd_state * pfsd;

/* Connectivity types. */

#define CONN_CNT 5

#define NOT_CONN 0x00
#define LAN_CONN 0x01
#define WAN_CONN 0x02
#define BTH_CONN 0x03
#define USB_CONN 0x04

/* Ports used. */

#define ADM_PRT 8383
#define LAN_PRT 8384
#define WAN_PRT 8385
#define BTH_PRT 8386
#define USB_PRT 8387

/*
 * GLOBAL STATE used for propagation knwoledge
 */

/* sd_sate. */
struct pfsd_sd
{
  char sd_id [PFS_ID_LEN];

  uint8_t conn;
  uint8_t dirty;

  struct pfsd_sd * next;
};


/* grp_state. */
struct pfsd_grp
{
  char grp_id [PFS_ID_LEN];
  uint32_t sd_cnt;

  struct pfsd_sd * sd;
};


/* pfsd global state. */
struct pfsd_state
{
  uint32_t grp_cnt;
  struct pfsd_grp * grp;     /* Grp state.           */
  struct pfs_mutex s_lock;
  
  struct pfsd_log_grp * log; /* Log structure.       */

  struct pfs_updt * updt;    /* Uncommited updates.  */
  struct pfs_mutex u_lock;
  
  int sock_c [CONN_CNT];     /* Connectivity sockets. */
  int sock_a;                /* Admin sockets.        */
};



int pfsd_state_read (struct pfsd_state * pfsd);
int pfsd_state_write (struct pfsd_state * pfsd);
int pfsd_state_free (struct pfsd_state * pfsd);

int pfsd_bootstrap (char * root_path);

#endif
