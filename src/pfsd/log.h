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

  struct pfsd_log_entry * next;
};



/* Latest updt for a given sd in a given dir. */
struct pfsd_log_sd_cnt
{
  uint32_t last;
  char sd_id [PFS_ID_LEN];

  struct pfsd_log_sd * next;
};



/* Updates and sd_cnt within a dir. */
struct pfsd_log_dir
{
  char dir_id [PFS_ID_LEN];
  struct pfsd_log_sd_cnt * sd_cnt;
  struct pfsd_log_entry * log;

  struct pfsd_log_dir * next;
};



/* grp log. */
struct pfsd_log_grp
{
  char grp_id [PFS_ID_LEN];
  struct pfsd_log_sd_cnt * sd_cnt;
  struct pfsd_log_dir * dir_log;

  struct pfs_mutex g_lock;

  struct pfsd_log_grp * next;
};


#endif
