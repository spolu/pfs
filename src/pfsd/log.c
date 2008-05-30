#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <signal.h>

#include "log.h"
#include "../libpfs/updt.h"
#include "../libpfs/entry.h"
#include "../libpfs/group.h"

int
pfsd_updt_log (struct pfsd_state * pfsd)
{
  struct pfs_updt * updt;
  struct pfs_updt * prev;
  struct pfs_updt * next;

  struct pfsd_grp_log * grp_log;
  struct pfs_group * grp;
  struct pfs_sd * sd;
  uint8_t prop;
  

  pfs_mutex_lock (&pfsd->log_lock);
      
  pfs_mutex_lock (&pfsd->updt_lock);

  while (pfsd->updt_cnt > 0)
    {
      updt = pfsd->updt;
      pfsd->updt = updt->next;
      pfsd->updt_cnt --;
      pfs_mutex_unlock (&pfsd->updt_lock);
      

      grp_log = pfsd->log->grp_log;
      while (grp_log != NULL) {
	if (strncmp (updt->grp_id, grp_log->grp_id, PFS_ID_LEN) == 0)
	  goto found_grp_log;
	grp_log = grp_log->next;
      }
      
      /* Grp not found we add it. */
      grp_log = (struct pfsd_grp_log *) malloc (sizeof (struct pfsd_grp_log));
      strncpy (grp_log->grp_id, updt->grp_id, PFS_ID_LEN);
      grp_log->log_cnt = 0;
      grp_log->log = NULL;
      grp_log->next = pfsd->log->grp_log;
      pfsd->log->grp_log = grp_log;
      pfsd->log->grp_cnt ++;
      
    found_grp_log:
      prev = NULL;
      next = grp_log->log;
      
      /* We remove the dominated log_entries. */
      while (next != NULL) {
	if (strncmp (next->dir_id, updt->dir_id, PFS_ID_LEN) == 0 &&
	    strncmp (next->name, updt->name, PFS_NAME_LEN) == 0)
	  {
	    if (pfs_vv_cmp (next->ver->mv, updt->ver->mv) == -1) {
	      grp_log->log_cnt --;
	      if (prev == NULL) {
		grp_log->log = next->next;
		pfs_free_updt (next);
		next = grp_log->log;
		continue;
	      }
	      else {
		prev->next = next->next;
		pfs_free_updt (next);
		next = prev->next;
		continue;
	      }
	    }
	    /* The update is dominated, we abort it. */
	    if (pfs_vv_cmp (next->ver->mv, updt->ver->mv) == 1)
	      goto abort;
	  }
	prev = next;
	next = next->next;
      }
      
      updt->next = NULL;
      if (grp_log->log == NULL) {
	grp_log->log = updt;
      }
      else {
	next = grp_log->log;
	while (next->next != NULL) {
	  next = next->next;
	}
	next->next = updt;
      }
	
      grp_log->log_cnt ++;
      printf ("ADDING UPDATE : updt_cnt %d\n", grp_log->log_cnt);
      goto done;
      
    abort:
      printf ("ABORTING ADD, dominated\n");
      pfs_free_updt (updt);

    done:
      pfs_mutex_lock (&pfsd->updt_lock);
    }
  pfs_mutex_unlock (&pfsd->updt_lock);

  goto finish;
  /* 
   * All updates have been inserted.
   * Let's remove propagated updates.
   */
  printf ("REMOVING PROPAGATED ENTRIES\n");

  pfs_mutex_lock (&pfsd->pfs->group_lock);
  
  grp_log = pfsd->log->grp_log;

  while (grp_log != NULL) {

    grp = pfsd->pfs->group;
    while (grp != NULL) {
      if (strncmp (grp->grp_id, grp_log->grp_id, PFS_ID_LEN) == 0)
	goto found_grp;
      grp = grp->next;
    }
    ASSERT (0);

  found_grp:
    prev = NULL;
    next = grp_log->log;
    
    while (next != NULL)
      {
	prop = 1;
	sd = grp->sd;
	while (sd != NULL) {
	  if (pfs_vv_cmp (sd->sd_sv, next->ver->mv) <= 0)
	    prop = 0;
	  sd = sd->next;
	}
	
	/* Update prop we remove it. */
	if (prop == 1) {
	  grp_log->log_cnt --;	  
	  if (prev == NULL) {
	    grp_log->log = next->next;
	    pfs_free_updt (next);
	    next = grp_log->log;
	    continue;
	  }
	  else {
	    prev->next = next->next;
	    pfs_free_updt (next);
	    next = prev->next;
	    continue;
	  }
	}
	  
	prev = next;
	next = next->next;
      }

    grp_log = grp_log->next;
  }
  pfs_mutex_unlock (&pfsd->pfs->group_lock);
  
  /* 
   * We have removed prop updates 
   * We have removed dominated updts 
   * We have added all pending updates 
   * we're done !
   */
  
 finish:
  pfs_mutex_unlock (&pfsd->log_lock);

  return 0;
}



int
pfsd_write_back_log (struct pfsd_state * pfsd)
{
  int fd;
  int i,j;
  struct pfsd_grp_log * grp_log;
  struct pfs_updt * log;

  pfs_mutex_lock (&pfsd->log_lock);
  ASSERT ((fd = open (pfsd->log_path, O_WRONLY|O_TRUNC|O_APPEND)) >= 0);
  
  writen (fd, &pfsd->log->grp_cnt, sizeof (uint32_t));
  i = 0;
  grp_log = pfsd->log->grp_log;
  while (grp_log != NULL)
    {
      i ++;
      writen (fd, grp_log->grp_id, PFS_ID_LEN);
      writen (fd, &grp_log->log_cnt, sizeof (uint32_t));

      j = 0;
      log = grp_log->log;
      while (log != NULL)
	{
	  j ++;
	  pfs_write_updt (fd, log);
	  log = log->next;
	}
      
      ASSERT (j == grp_log->log_cnt);
      grp_log = grp_log->next;
    }
  ASSERT (i == pfsd->log->grp_cnt);

  close (fd);
  pfs_mutex_unlock (&pfsd->log_lock);

  return 0;
}



int
pfsd_print_log (struct pfsd_state * pfsd)
{
  struct pfsd_grp_log * grp_log;
  struct pfs_updt * log;

  pfs_mutex_lock (&pfsd->log_lock);
  printf ("\n\n******* PFSD_LOG ********\n");
  grp_log = pfsd->log->grp_log;
  while (grp_log != NULL)
    {
      printf ("GROUP : %.*s\n", PFS_ID_LEN, grp_log->grp_id);
      printf ("UPDT_CNT : %d\n", grp_log->log_cnt);
      log = grp_log->log;
      while (log != NULL)
	{
	  pfs_print_updt (log);
	  log = log->next;
	}
      
      grp_log = grp_log->next;
    }
  pfs_mutex_unlock (&pfsd->log_lock);
  return 0;
}



int
pfsd_read_log (struct pfsd_state * pfsd)
{
  int fd;
  int i,j;
  struct pfsd_grp_log * grp_log;
  struct pfs_updt * log;
  struct pfs_updt * next;

  pfs_mutex_lock (&pfsd->log_lock);

  ASSERT ((fd = open (pfsd->log_path, O_RDONLY)) >= 0);

  ASSERT (pfsd->log->grp_log == NULL);
  readn (fd, &pfsd->log->grp_cnt, sizeof (uint32_t));

  for (i = 0; i < pfsd->log->grp_cnt; i ++)
    {
      grp_log = (struct pfsd_grp_log *) malloc (sizeof (struct pfsd_grp_log));
      readn (fd, grp_log->grp_id, PFS_ID_LEN);
      readn (fd, &grp_log->log_cnt, sizeof (uint32_t));
      grp_log->log = NULL;
      grp_log->next = pfsd->log->grp_log;
      pfsd->log->grp_log = grp_log;

      next = NULL;
      for (j = 0; j < grp_log->log_cnt; j ++) 
	{
	  log = pfs_read_updt (fd);

	  /* We keep the order. */
	  log->next = NULL;
	  if (next == NULL)
	    grp_log->log = log;
	  else
	    next->next = log;
	  next = log;
	}      
    }

  close (fd);
  pfs_mutex_unlock (&pfsd->log_lock);

  return 0;
}

