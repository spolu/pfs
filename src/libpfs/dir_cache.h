/*
 * Directory caching 
 *
 * Copyright (C) 2008 Stanislas Polu <spolu@stanford.edu>. 
 * All Rights Reserved.
 */

#ifndef _PFS_DIR_CACHE_H
#define _PFS_DIR_CACHE_H

#include <time.h>
#include <stdint.h>

#include "lib/hashtable.h"
#include "lib/lock.h"

#define DIR_CACHE_SIZE 512

struct pfs_instance;

typedef struct pfs_dir_cache {
  ht_t * ht;
  struct pfs_mutex lock;
} pdc_t;

typedef struct pfs_dir_cache_entry {
  struct pfs_dir * dir;
  time_t atime;
  int usec;
  uint8_t dirty;
} pdce_t;


int pfs_init_dir_cache (struct pfs_instance * pfs);

int pfs_destroy_dir_cache (struct pfs_instance * pfs);

struct pfs_dir * pfs_get_dir_cache (struct pfs_instance * pfs,
				    const char * dir_id);		       

int pfs_unlock_dir_cache (struct pfs_instance * pfs,
			  struct pfs_dir * dir);

int pfs_dirty_dir_cache (struct pfs_instance * pfs,
			 const char * dir_id);

int pfs_create_dir_cache (struct pfs_instance * pfs,
			  char * dir_id);

int pfs_create_dir_cache_with_id (struct pfs_instance * pfs,
				  const char * dir_id);

int pfs_remove_dir_cache (struct pfs_instance * pfs,
			  const char * dir_id);

int pfs_sync_dir_cache (struct pfs_instance * pfs);

int pfs_write_back_dir_cache (struct pfs_instance * pfs,
			      const char * dir_id);


#endif
