/*
 * pFS Daemon : log structure
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFSD_LOG_H
#define _PFSD_LOG_H

#include "../libpfs/updt.h"
#include "pfsd.h"

/*
 * LOG STRUCTURE
 */

/* grp log. */
struct pfsd_grp_log
{
  char grp_id [PFS_ID_LEN];

  uint32_t log_cnt;
  struct pfs_updt * log;       /* Ordered by arrival. */
  struct pfsd_grp_log * next;
};


/* log. */
struct pfsd_log
{
  uint32_t grp_cnt;
  struct pfsd_grp_log * grp_log;
};

int pfsd_print_log (struct pfsd_state * pfsd);
int pfsd_updt_log (struct pfsd_state * pfsd);
int pfsd_write_back_log (struct pfsd_state * pfsd);
int pfsd_read_log (struct pfsd_state * pfsd);

#endif
