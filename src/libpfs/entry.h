/*
 * Directory, entry, version management
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_ENTRY_H
#define _PFS_ENTRY_H

#include "instance.h"
#include "lib/lock.h"

struct pfs_dir
{
  char type [3];                  /* "DIR" to identify dirs.     */
  char id [PFS_ID_LEN];           /* Hash (creator, incr_token). */
  uint32_t entry_cnt;             /* Entry count.                */
  struct pfs_entry ** entry;      /* Pfs entries.                */
  struct pfs_mutex lock;          /* Used path_cache.            */
};



struct pfs_entry
{
  char name [PFS_NAME_LEN];       /* Directory entry name. */
  uint32_t main_idx;              /* Main version index.   */
  uint32_t ver_cnt;               /* Version count.        */
  struct pfs_ver ** ver;          /* Entry verisons.       */
};



enum pfs_entry_type {
  PFS_DEL = 0,                         /* Deleted file.         */
  PFS_DIR = 1,                         /* Directory entry.      */
  PFS_SML = 2,                         /* Symbolic link entry.  */
  PFS_FIL_PRST = 3,                    /* File present entry.   */
  PFS_FIL_INCH = 4,                    /* File in_charge entry. */
  PFS_FIL_EVCT = 5,                    /* File evicted entry.   */
  PFS_GRP = 6
};

struct pfs_ver
{
  uint8_t type;                   /* entry_ver type.                */
  char dst_id [PFS_ID_LEN];       /* id of the object it points to. */
  struct pfs_vv * vv;             /* version vector for that entry. */
};

struct pfs_vv
{ 
  char last_updt [PFS_ID_LEN];    /* Id of the last_updater sd.           */
  uint32_t len;                   /* Len of the version vector.           */
  char ** sd_id;                  /* Ids of sds associated with versions. */
  uint32_t * value;               /* Versions value.                      */
};




/* ENTRY OPERATIONS */

int pfs_create_dir (struct pfs_instance * pfs,
		    char * dir_id, 
		    uint8_t gen_id);

int pfs_dir_rmdir (struct pfs_instance *pfs,
		   const char * dir_id);
  
uint8_t pfs_dir_empty (struct pfs_instance *pfs,
		       char * dir_id);

int pfs_set_entry (struct pfs_instance * pfs,
		   const char * grp_id,
		   const char * dir_id,
		   const char * name,
		   const uint8_t reclaim,
		   const struct pfs_ver * ver);

struct pfs_entry * pfs_get_entry (struct pfs_instance * pfs,
				  char * dir_id,
				  char * name);

/* VV OPERATIONS */

int pfs_vv_cmp (struct pfs_vv * a,
		struct pfs_vv * b);

struct pfs_vv * pfs_vv_merge (struct pfs_instance * pfs,
			      struct pfs_vv * a,
			      struct pfs_vv * b);

int pfs_vv_incr (struct pfs_instance * pfs,
		  struct pfs_vv * vv);


/* DEBUG OPERATIONS */

void pfs_print_entry (struct pfs_instance * pfs,
		      char * dir_id,
		      char * name);

/* DEALLOC/CPY OPERATIONS */

struct pfs_vv * pfs_cpy_vv (struct pfs_vv * vv);
struct pfs_ver * pfs_cpy_ver (const struct pfs_ver * ver);
struct pfs_entry * pfs_cpy_entry (struct pfs_entry * entry);

void pfs_free_vv (struct pfs_vv * vv);
void pfs_free_ver (struct pfs_ver * ver);
void pfs_free_entry (struct pfs_entry * entry);
void pfs_free_dir (struct pfs_dir * dir);

#endif  /* _PFS_ENTRY_H */
