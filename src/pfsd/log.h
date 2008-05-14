/*
 * pFS Daemon : log structure
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFSD_LOG_H
#define _PFSD_LOG_H


/*
 * LOG STRUCTURE
 */


/* Log entry. */
struct pfsd_log_entry
{
  char sd_orig [PFS_ID_LEN];
  uint32_t orig_cnt;
  struct pfs_updt * updt;
};

struct pfsd_log
{
};

#endif
