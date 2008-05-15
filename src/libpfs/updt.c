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

int pfs_push_log_entry (struct pfs_instance *pfs,
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
  int j;

  printf ("SET_ENTRY\n%.*s\n%.*s\n%s (", PFS_ID_LEN, updt->grp_id, PFS_ID_LEN, updt->dir_id, updt->name);
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
  printf (") reclaim : %d\nlast_updt : %.*s \n", updt->reclaim, PFS_ID_LEN, updt->ver->last_updt);
  for (j = 0; j < updt->ver->mv->len; j ++) {
    printf ("  %.*s : %d\n", PFS_ID_LEN, updt->ver->mv->sd_id[j], (int) updt->ver->mv->value[j]);
  }
}
