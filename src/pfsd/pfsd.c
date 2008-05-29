/*
 * pFS Daemon
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "pfsd.h"
#include "fuse/pfs_fuse.h"
#include "log.h"
#include "srv.h"

int
main (int argc, char ** argv)
{
  pthread_t wb_thread;
  pthread_t srv_thread;
  struct pfs_instance * pfs;
  char root_path [PFS_NAME_LEN];  

  if (argc <= 2) {
    printf ("usage : ./pfs root_path [FUSE ARGS]\n");
    exit (1);
  }

  strncpy (root_path, argv[1], PFS_NAME_LEN);
  pfs = pfs_init (root_path);
  strncpy (argv[1], argv[0], strlen (argv[1]));
  argv ++;
  argc --;

  pfs_set_updt_cb (pfs, updt_cb);
  pfsd_init (pfs);

  if (pthread_create (&wb_thread, NULL, start_write_back, (void *)0) != 0) {
    printf ("Could not spawn write back thread.\n");
    exit (1);
  }  

  if (pthread_create (&srv_thread, NULL, start_srv, (void *)0) != 0) {
    printf ("Could not spawn pfsd_srv thread.\n");
    exit (1);
  }  

  fuse_main (argc, argv, &pfs_operations, NULL);
  
  exit (0);
}


void *
start_write_back (void * tid)
{
  while (1) {
    sleep (WRITE_BACK_SLEEP);
    pfs_sync (pfsd->pfs);
    pfsd_updt_log (pfsd);
    pfsd_write_back_log (pfsd);
    pfsd_print_log (pfsd);
  }
}

int
updt_cb (struct pfs_instance * pfs,
	 struct pfs_updt * updt)
{
  struct pfs_updt * new_updt;
  new_updt = pfs_cpy_updt (updt);
  
  pfs_mutex_lock (&pfsd->log_lock);
  new_updt->next = pfsd->updt;
  pfsd->updt = new_updt;  
  pfsd->updt_cnt += 1;
  pfs_mutex_unlock (&pfsd->log_lock);

  pfs_print_updt (pfsd->updt);

  return 0;
}


int
pfsd_init (struct pfs_instance * pfs)
{
  pfsd = (struct pfsd_state *) malloc (sizeof (struct pfsd_state));

  pfs_mutex_init (&pfsd->log_lock);
  pfs_mutex_init (&pfsd->updt_lock);
  pfs_mutex_init (&pfsd->sd_lock);

  pfsd->pfs = pfs;

  pfsd->updt_cnt = 0;
  pfsd->updt = NULL;
  
  pfsd->sd_cnt = 0;
  pfsd->sd = NULL;

  pfsd->log_path = (char *) malloc (strlen (pfsd->pfs->root_path) + 
			      strlen (PFSD_LOG_PATH) + 1);
  sprintf (pfsd->log_path, "%s%s", pfsd->pfs->root_path, PFSD_LOG_PATH);

  pfsd->log = (struct pfsd_log *) malloc (sizeof (struct pfsd_log));
  pfsd->log->grp_log = NULL;
  pfsd->log->grp_cnt = 0;
  pfsd_read_log (pfsd);

  return 0;
}

int pfsd_destroy ()
{ 
  pfsd_updt_log (pfsd);
  pfsd_write_back_log (pfsd);

  pfs_mutex_destroy (&pfsd->log_lock);
  pfs_mutex_destroy (&pfsd->updt_lock);
  pfs_mutex_destroy (&pfsd->sd_lock);

  pfs_destroy (pfsd->pfs);
  
  return 0;
}

