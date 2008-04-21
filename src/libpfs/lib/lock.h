#ifndef _PFS_LOCK_H
#define _PFS_LOCK_H

#ifdef PFS_PTHREAD_LOCK
#include "pthread.h"
#endif

/*
  PFS MUTEX implementation depends on global definition.
 */

struct pfs_mutex {
#ifdef PFS_PTHREAD_LOCK
  pthread_mutex_t mutex;
#endif
};


int pfs_mutex_init (struct pfs_mutex * mutex);
int pfs_mutex_lock (struct pfs_mutex * mutex);
int pfs_mutex_unlock (struct pfs_mutex * mutex);
int pfs_mutex_destroy (struct pfs_mutex * mutex);

#endif 
