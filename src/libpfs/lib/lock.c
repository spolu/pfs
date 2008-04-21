#include "lock.h"

int
pfs_mutex_init (struct pfs_mutex * mutex)
{
#ifdef PFS_PTHREAD_LOCK
  return pthread_mutex_init (&mutex->mutex, NULL);
#endif
  return 0;
}

int
pfs_mutex_lock (struct pfs_mutex * mutex)
{
#ifdef PFS_PTHREAD_LOCK
  return pthread_mutex_lock (&mutex->mutex);
#endif  
  return 0;
}


int
pfs_mutex_unlock (struct pfs_mutex * mutex)
{
#ifdef PFS_PTHREAD_LOCK
  return pthread_mutex_unlock (&mutex->mutex);
#endif  
  return 0;
}


int
pfs_mutex_destroy (struct pfs_mutex * mutex)
{
#ifdef PFS_PTHREAD_LOCK
  return pthread_mutex_destroy (&mutex->mutex);
#endif  
  return 0;
}
