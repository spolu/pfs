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
#include <signal.h>

#include "pfsd.h"
#include "fuse/pfs_fuse.h"
#include "log.h"
#include "srv.h"
#include "prop.h"

int
main (int argc, char ** argv)
{
  pthread_t wb_thread;
  pthread_t srv_thread;
  pthread_t updt_thread;
  struct pfs_instance * pfs;
  char root_path [PFS_NAME_LEN];  
  int pfsd_port;

  if (argc != 4) {
    printf ("usage : ./pfsd port root_path dest_path\n");
    exit (1);
  }

  pfsd_port = atoi (argv[1]);
  strncpy (root_path, argv[2], PFS_NAME_LEN);
  pfs = pfs_init (root_path);
    
  pfs_set_updt_cb (pfs, updt_cb);
  pfsd_init (pfs, pfsd_port);

  if (pthread_create (&wb_thread, NULL, start_write_back, (void *)0) != 0) {
    printf ("Could not spawn write back thread.\n");
    exit (1);
  }  

  if (pthread_create (&updt_thread, NULL, commit_updt, (void *)0) != 0) {
    printf ("Could not spawn commit updt thread.\n");
    exit (1);
  }  

  if (pthread_create (&srv_thread, NULL, start_srv, (void *)0) != 0) {
    printf ("Could not spawn pfsd_srv thread.\n");
    exit (1);
  }  

  
  /* Starting TUN. */

  char * args[5];
  args[0] = "lan_tun.py";
  args[2] = pfs->sd_owner;
  args[3] = pfs->sd_name;
  char str1[PFS_ID_LEN + 1];
  sprintf (str1, "%.*s", PFS_ID_LEN, pfs->sd_id);
  args[1] = str1;
  char str2 [10];
  sprintf (str2, "%d", pfsd_port);
  args[4] = str2;
  
  if ((pfsd->tun_pid = fork ()) == 0) {
    execlp ("lan_tun.py", args[0], args[1], args[2], args[3], args[4], NULL);
    exit (0);
  }

  /* Starting FUSE. */

  char * fuse_args [9];
  int fuse_argc = 9;
  fuse_args[0] = "./pfsd";
  fuse_args[1] = argv[3];
  fuse_args[2] = "-f";
  fuse_args[3] = "-o";
  fuse_args[4] = (char *) malloc (strlen (pfs->sd_owner) +
				  strlen (pfs->sd_name) +
				  20);
  sprintf (fuse_args[4], "volname=pFS.%s.%s", pfs->sd_owner, pfs->sd_name);
  fuse_args[5] = "-o";
  fuse_args[6] = "defer_permissions";
  fuse_args[7] = "-o";
  fuse_args[8] = "nolocalcaches";
 

  fuse_main (fuse_argc, fuse_args, &pfs_operations, NULL);
  
  exit (0);
}


void *
start_write_back (void * tid)
{
  while (1) {
    sleep (WRITE_BACK_SLEEP);
    pfs_sync (pfsd->pfs);
  }
}


void *
commit_updt (void * tid)
{
  while (1) {
    if (pfsd->update == 2) {
      pfsd_updt_log (pfsd);
      pfsd_write_back_log (pfsd);
      //pfsd_print_log (pfsd);
      show_time ();
      if (pfsd->update == 2)
	pfsd->update = 0;
    } 
    if (pfsd->update == 1) {
      pfsd->update = 2;
    }
    sleep (COMMIT_UPDT_SLEEP);
  }
}

int
updt_cb (struct pfs_instance * pfs,
	 struct pfs_updt * updt)
{
  struct pfs_updt * new_updt;
  struct pfs_updt * next;

  if (updt == NULL) {
    pfsd->update = 1;
    return 0;
  }

  new_updt = pfs_cpy_updt (updt);
  
  pfs_mutex_lock (&pfsd->updt_lock);
  
  new_updt->next = NULL;
  if (pfsd->updt == NULL)
    pfsd->updt = new_updt; 
  else {
    next = pfsd->updt;
    while (next->next != NULL)
      next = next->next;
    next->next = new_updt;
  }
  pfsd->updt_cnt += 1;
  pfs_mutex_unlock (&pfsd->updt_lock);

  pfsd->update = 1;

  return 0;
}


int
pfsd_init (struct pfs_instance * pfs, int pfsd_port)
{
  pfsd = (struct pfsd_state *) malloc (sizeof (struct pfsd_state));

  pfs_mutex_init (&pfsd->log_lock);
  pfs_mutex_init (&pfsd->updt_lock);
  pfs_mutex_init (&pfsd->sd_lock);

  pfsd->pfs = pfs;
  pfsd->pfsd_port = pfsd_port;

  pfsd->update = 0;

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

  printf ("sending signal to %d\n", pfsd->tun_pid);
  kill (pfsd->tun_pid, SIGTSTP);

  sleep (2);

  pfs_mutex_destroy (&pfsd->log_lock);
  pfs_mutex_destroy (&pfsd->updt_lock);
  pfs_mutex_destroy (&pfsd->sd_lock);

  pfs_destroy (pfsd->pfs);
  

  return 0;
}

