/*
 * Update callback
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <signal.h>
#include <time.h>

#include "updt.h"
#include "entry.h"


/*---------------------------------------------------------------------
 * Method: pfs_push_log_entry
 * Scope:  Global
 *
 * Callback the updt_cb function to pfsd.
 *
 *---------------------------------------------------------------------*/

int pfs_push_updt (struct pfs_instance *pfs,
		   const char * grp_id,
		   const char * dir_id,
		   const char * name,
		   const uint8_t reclaim,
		   const struct pfs_ver * ver)
{
  struct pfs_updt * updt;
  
  updt = (struct pfs_updt *) malloc (sizeof (struct pfs_updt));
  
  strncpy (updt->grp_id, grp_id, PFS_ID_LEN);
  strncpy (updt->dir_id, dir_id, PFS_ID_LEN);
  strncpy (updt->name, name, PFS_NAME_LEN);
  updt->reclaim = reclaim;
  updt->ver = pfs_cpy_ver (ver);  
  updt->next = NULL;

  if (pfs->updt_cb != NULL)
    pfs->updt_cb (pfs, updt);

  pfs_free_updt (updt);

  return 0;
}



/*---------------------------------------------------------------------
 * Method: pfs_(read|write|free)_updt
 * Scope:  Global
 *
 * Serialization function.
 *
 *---------------------------------------------------------------------*/

void pfs_free_updt (struct pfs_updt * updt)
{
  pfs_free_ver (updt->ver);
  free (updt);
}


struct pfs_updt * pfs_cpy_updt (const struct pfs_updt * updt)
{
  struct pfs_updt * new_updt;

  new_updt = (struct pfs_updt *) malloc (sizeof (struct pfs_updt));
  ASSERT (new_updt != NULL);

  memcpy (new_updt->grp_id, updt->grp_id, PFS_ID_LEN);
  memcpy (new_updt->dir_id, updt->dir_id, PFS_ID_LEN);
  strncpy (new_updt->name, updt->name, PFS_NAME_LEN);
  new_updt->reclaim = updt->reclaim;
  new_updt->ver = pfs_cpy_ver (updt->ver);
  new_updt->next = updt->next;

  return new_updt;
}

/*---------------------------------------------------------------------
 * Method: pfs_print_entry
 * Scope:  Global
 *
 * Prints this entry versions
 *
 *---------------------------------------------------------------------*/

void pfs_print_updt (struct pfs_updt * updt)
{
  printf ("*---*\nSET_ENTRY\n%.*s\n%.*s\n%.*s\n%s (", PFS_ID_LEN, updt->grp_id, PFS_ID_LEN, updt->dir_id, PFS_ID_LEN, updt->ver->dst_id, updt->name);
  switch (updt->ver->type) {
  case PFS_DIR:
    printf ("DIR");
    break;
  case PFS_FIL:
    printf ("FIL");
    break;
  case PFS_SML:
    printf ("SML");
    break;
  case PFS_DEL:
    printf ("DEL");
    break;
  }
  printf (") reclaim : %d\n LU : %.2s \n", updt->reclaim, updt->ver->last_updt);
  printf ("NV : ");
  pfs_print_vv (updt->ver->mv);
  printf ("CS : %.2s:%d\n", updt->ver->sd_orig, (int) updt->ver->cs);
}


struct pfs_updt *
pfs_read_updt (int fd)
{
  struct pfs_updt * updt = (struct pfs_updt *) malloc (sizeof (struct pfs_updt));
  readn (fd, updt->grp_id, PFS_ID_LEN);
  readn (fd, updt->dir_id, PFS_ID_LEN);
  readn (fd, updt->name, PFS_NAME_LEN);
  readn (fd, &updt->reclaim, sizeof (uint8_t));
  updt->ver = pfs_read_ver (fd);
  updt->next = NULL;
  return updt;
}


int
pfs_write_updt (int fd,
		const struct pfs_updt * updt)
{
  writen (fd, updt->grp_id, PFS_ID_LEN);
  writen (fd, updt->dir_id, PFS_ID_LEN);
  writen (fd, updt->name, PFS_NAME_LEN);
  writen (fd, &updt->reclaim, sizeof (uint8_t));
  pfs_write_ver (fd, updt->ver);
  return 0;
}
