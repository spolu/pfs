#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/file.h>

#include "dir_cache.h"
#include "instance.h"
#include "entry.h"

static int ht_pdce_cmp (const void *d1, const void *d2);
static unsigned long ht_id_hash (const void * key);
static void pfs_free_pdce (void * val);
static int pfs_dir_cache_evict (struct pfs_instance *pfs);

/* SERIALIZATION FUNCTION. */

static void pfs_write_vv (int wd, struct pfs_vv * vv);
static void pfs_write_ver (int wd, const struct pfs_ver * ver);
static void pfs_write_entry (int wd, struct pfs_entry * entry);
static void pfs_write_dir (int wd, struct pfs_dir * dir);

static struct pfs_vv * pfs_read_vv (int rd);
static struct pfs_ver * pfs_read_ver (int rd);
static struct pfs_entry * pfs_read_entry (int rd);
static struct pfs_dir * pfs_read_dir (int rd);



/*---------------------------------------------------------------------
 * Method: pfs_init_dir_cache
 * Scope: Global
 *
 *---------------------------------------------------------------------*/

int 
pfs_init_dir_cache (struct pfs_instance * pfs)
{
  pfs->dir_cache = (pdc_t *) malloc (sizeof (pdc_t));
  pfs->dir_cache->ht = ht_create (DIR_CACHE_SIZE / 2);
  ht_set_key_cmp (pfs->dir_cache->ht,
		  ht_pdce_cmp);
  ht_set_hash (pfs->dir_cache->ht,
	       ht_id_hash);
  ht_set_dealloc (pfs->dir_cache->ht,
		  NULL,
		  pfs_free_pdce);
  pfs_mutex_init (&pfs->dir_cache->lock);
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_destroy_dir_cache
 * Scope: Global
 *
 * Calls pfs_write_back_cache
 *
 *---------------------------------------------------------------------*/

int
pfs_destroy_dir_cache (struct pfs_instance * pfs)
{
  pfs_sync_dir_cache (pfs);
  pfs_mutex_lock (&pfs->dir_cache->lock);
  pfs_mutex_unlock (&pfs->dir_cache->lock);
  ht_destroy (pfs->dir_cache->ht);
  pfs_mutex_destroy (&pfs->dir_cache->lock);
  free (pfs->dir_cache);
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_write_back_dir_cache
 * Scope: Global
 *
 * Writes back a specific dir. Used for pfs_fsync
 * 
 *---------------------------------------------------------------------*/

int 
pfs_write_back_dir_cache (struct pfs_instance * pfs,
			  const char * dir_id)
{
  pdce_t * val;
  char * dir_path;
  int fd;

  pfs_mutex_lock (&pfs->dir_cache->lock);

  if ((val = ht_get (pfs->dir_cache->ht, dir_id)) != NULL) {
    
    pfs_mutex_lock (&val->dir->lock);

    if (val->dirty != 0) {
      dir_path = pfs_mk_dir_path (pfs, dir_id);	  
      
      if (dir_path == NULL) {
	pfs_mutex_unlock (&val->dir->lock);
	pfs_mutex_unlock (&pfs->dir_cache->lock);
	return -1;
      }
      
      if ((fd = open (dir_path, O_WRONLY|O_TRUNC)) < 0) {
	free (dir_path);
	pfs_mutex_unlock (&val->dir->lock);
	pfs_mutex_unlock (&pfs->dir_cache->lock);
	return -1;
      }
      free (dir_path);
      
      flock (fd, LOCK_EX);
      printf ("Writing back dir : %.*s\n", PFS_ID_LEN, val->dir->id);
      pfs_write_dir (fd, val->dir);
      flock (fd, LOCK_UN);
      close (fd);
      val->dirty = 0;
    } 
    
    pfs_mutex_unlock (&val->dir->lock);
  }
  pfs_mutex_unlock (&pfs->dir_cache->lock);
  
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_sync_dir_cache
 * Scope: Global
 *
 * Syncs back the whole content of the cache
 *
 *---------------------------------------------------------------------*/

int pfs_sync_dir_cache (struct pfs_instance * pfs)
{
  int i, fd;
  pdce_t * val;
  char * dir_path;
  ht_pair_t * pair;

  pfs_mutex_lock (&pfs->dir_cache->lock);
  
  for (i = 0; i < pfs->dir_cache->ht->num_buck; i++) {
    pair = pfs->dir_cache->ht->bucks[i];
    
    while (pair != NULL) {
      val = (pdce_t *) pair->value;
      pfs_mutex_lock (&val->dir->lock);
      
      if (val->dirty != 0) {
	dir_path = pfs_mk_dir_path (pfs, val->dir->id);	  
	
	if (dir_path == NULL) {
	  pfs_mutex_unlock (&val->dir->lock);
	  pair = pair->next;
	  continue;
	}
	
	if ((fd = open (dir_path, O_WRONLY|O_TRUNC)) < 0) {
	  free (dir_path);
	  pfs_mutex_unlock (&val->dir->lock);
	  pair = pair->next;
	  continue;
	}
	free (dir_path);
	
	flock (fd, LOCK_EX);
	printf ("Writing back dir : %.*s\n", PFS_ID_LEN, val->dir->id);
	pfs_write_dir (fd, val->dir);
	flock (fd, LOCK_UN);
	close (fd);
	val->dirty = 0;
      }
 
      pfs_mutex_unlock (&val->dir->lock);
      pair = pair->next;
    }
  }
  
  pfs_mutex_unlock (&pfs->dir_cache->lock);

  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_get_dir_cache
 * Scope: Global
 *
 * Get pfs_dir from cache or retrieve it. Evict using LRU if
 * num_elem >= DIR_CACHE_SIZE. Atomically Acquire a lock
 * on the dir.
 *
 *---------------------------------------------------------------------*/

struct pfs_dir * pfs_get_dir_cache (struct pfs_instance * pfs,
				    const char * dir_id)
{
  pdce_t * val;
  char * dir_path;
  int fd;
  struct timeval tv;

  pfs_mutex_lock (&pfs->dir_cache->lock);

  if ((val = ht_get (pfs->dir_cache->ht, dir_id)) == NULL) {

    pfs_dir_cache_evict (pfs);
  
    val = (pdce_t *) malloc (sizeof (pdce_t));
    dir_path = pfs_mk_dir_path (pfs, dir_id);
    
    if (dir_path == NULL) {
      pfs_mutex_unlock (&pfs->dir_cache->lock);
      return NULL;
    }
    
    if ((fd = open (dir_path, O_RDONLY)) < 0) {
      free (dir_path);
      pfs_mutex_unlock (&pfs->dir_cache->lock);
      return NULL;
    }
    free (dir_path);
    flock (fd, LOCK_SH);
    val->dir = pfs_read_dir (fd);
    flock (fd, LOCK_UN);
    close (fd);

    pfs_mutex_init (&val->dir->lock);
    val->dirty = 0;
    
    ht_put (pfs->dir_cache->ht, val->dir->id, val);
  }

  pfs_mutex_lock (&val->dir->lock);
  pfs_mutex_unlock (&pfs->dir_cache->lock);

  gettimeofday (&tv, NULL);
  val->atime = time (NULL);
  val->usec = tv.tv_usec;

  return val->dir;
}

/*---------------------------------------------------------------------
 * Method: pfs_unlock_dir_cache
 * Scope: Global
 *
 *---------------------------------------------------------------------*/

int pfs_unlock_dir_cache (struct pfs_instance * pfs,
			  struct pfs_dir * dir)
{
  pfs_mutex_unlock (&dir->lock);
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_dirty_dir_cache
 * Scope: Global
 *
 * Set a dir as dirty. The lock on dir must have been released
 * Before calling this function.
 *
 *---------------------------------------------------------------------*/

int pfs_dirty_dir_cache (struct pfs_instance * pfs,
			 const char * dir_id)
{
  pdce_t * val;

  pfs_mutex_lock (&pfs->dir_cache->lock);

  if ((val = ht_get (pfs->dir_cache->ht, dir_id)) != NULL) {
    pfs_mutex_lock (&val->dir->lock);
    val->dirty = 1;
    pfs_mutex_unlock (&val->dir->lock);
    pfs_mutex_unlock (&pfs->dir_cache->lock);
    return 0;
  }
  pfs_mutex_unlock (&pfs->dir_cache->lock);
  return -1;
}


/*---------------------------------------------------------------------
 * Method: pfs_create_dir_cache
 * Scope: Global
 *
 * Does not acquire the lock on the newly created dir
 *
 *---------------------------------------------------------------------*/

int pfs_create_dir_cache (struct pfs_instance * pfs,
			  char * dir_id,
			  uint8_t gen_id)
{
  pdce_t * val;
  char * dir_path;
  int fd;
  struct timeval tv;

  val = (pdce_t *) malloc (sizeof (pdce_t));
  val->dir = (struct pfs_dir *) malloc (sizeof (struct pfs_dir));
  val->dirty = 0;

  gettimeofday (&tv, NULL);
  val->atime = time (NULL);
  val->usec = tv.tv_usec;

  pfs_mutex_init (&val->dir->lock);
  
  if (gen_id == 1)
    pfs_mk_id (pfs, val->dir->id);
  else
    memcpy (val->dir->id, dir_id, PFS_ID_LEN);
  memcpy (val->dir->type, "DIR", 3);
  val->dir->entry_cnt = 0;
  val->dir->entry = NULL;

  dir_path = pfs_mk_dir_path (pfs, val->dir->id);
  if ((fd = open (dir_path, O_WRONLY|O_APPEND|O_TRUNC|O_CREAT)) < 0) {
    free (dir_path);
    pfs_free_pdce (val);
    return -EIO;
  }  
  free (dir_path);

  flock (fd, LOCK_EX);
  pfs_write_dir (fd, val->dir);
  fchmod (fd, S_IRUSR | S_IWUSR);
  flock (fd, LOCK_UN);
  if (close (fd) < 0) {
    pfs_free_pdce (val);    
    return -errno;
  }

  if (gen_id == 1)
    memcpy (dir_id, val->dir->id, PFS_ID_LEN);
  
  pfs_mutex_lock (&pfs->dir_cache->lock);  
  pfs_dir_cache_evict (pfs);
  ht_put (pfs->dir_cache->ht, val->dir->id, val);
  pfs_mutex_unlock (&pfs->dir_cache->lock);  
  
  return 0;
}

/*---------------------------------------------------------------------
 * Method: pfs_remove_dir_cache
 * Scope: Global
 *
 * Remove the dir from the cache and remove the underlying dir
 *
 *---------------------------------------------------------------------*/

int
pfs_remove_dir_cache (struct pfs_instance *pfs,
		      const char * dir_id)
{
  char * dir_path;

  pfs_mutex_lock (&pfs->dir_cache->lock);
  ht_remove (pfs->dir_cache->ht, dir_id);
  pfs_mutex_unlock (&pfs->dir_cache->lock);  
  
  dir_path = pfs_mk_dir_path (pfs, dir_id);
  if (unlink (dir_path) < 0) {
    free (dir_path);
    return -errno;
  }
  free (dir_path);

  return 0;
}



/* STATIC FUNTIONS */


/*---------------------------------------------------------------------
 * Method: pfs_dir_cache_evict
 * Scope: Static
 *
 * Evict if necessary some entry. pdc_t->lock must be held
 *
 *---------------------------------------------------------------------*/

static int
pfs_dir_cache_evict (struct pfs_instance *pfs)
{
  pdce_t * val;
  char * dir_path;
  int fd, i;

  //printf ("EVICTION ENTER\n");
  /*
   * We evict the LRU pcde if num_elem >= DIR_CACHE_SIZE
   */
  while (pfs->dir_cache->ht->num_elem >= DIR_CACHE_SIZE) {
    val = NULL;
    for (i = 0; i < pfs->dir_cache->ht->num_buck; i++) {
      ht_pair_t *pair = pfs->dir_cache->ht->bucks[i];
      while (pair != NULL) {
	if (val == NULL || 
	    (((pdce_t *) pair->value)->atime < val->atime &&
	     ((pdce_t *) pair->value)->usec < val->usec)) {
	  val = ((pdce_t *) pair->value);
	}
	pair = pair->next;
      }
    }
    
    //printf ("EVICTION : %.*s\n", PFS_ID_LEN, val->dir->id);

    pfs_mutex_lock (&val->dir->lock);
    
    if (val->dirty != 0) {
      dir_path = pfs_mk_dir_path (pfs, val->dir->id);	  
      
      if (dir_path == NULL) {
	pfs_mutex_unlock (&val->dir->lock);
	return -1;
      }
      
      if ((fd = open (dir_path, O_WRONLY|O_TRUNC)) < 0) {
	free (dir_path);
	pfs_mutex_unlock (&val->dir->lock);
	return -1;
      }
      free (dir_path);
	
      flock (fd, LOCK_EX);
      pfs_write_dir (fd, val->dir);
      flock (fd, LOCK_UN);
      close (fd);
      val->dirty = 0;
    } 

    pfs_mutex_unlock (&val->dir->lock);

    /* 
     * The global ht lock is held... nobody can mess with this entry
     * before we remove it once and for all !
     */
    ht_remove (pfs->dir_cache->ht, val->dir->id);
  }
  
  return 0;
}

/*---------------------------------------------------------------------
 * Method: ht_pdce_cmp
 * Scope: Static
 *
 * Function used with lib/hashtable.c
 *
 *---------------------------------------------------------------------*/

static int
ht_pdce_cmp (const void *d1, const void *d2)
{
  int retval;
  retval = strncmp ((char *) d1, (char *) d2, PFS_ID_LEN);
  return retval;
}


/*---------------------------------------------------------------------
 * Method: ht_id_hash
 * Scope: Static
 *
 * Function used with lib/hashtable.c
 *
 *---------------------------------------------------------------------*/

static unsigned long
ht_id_hash (const void * key)
{
  const char *str = (const char *) key;
  unsigned long hash_val = 0;
  int i;

  for (i = 0; i < PFS_ID_LEN; i ++)
    hash_val = hash_val * 37 + str[i];

  return hash_val;
}

/*---------------------------------------------------------------------
 * Method: ht_free_pdce
 * Scope: Static
 *
 * Function used with lib/hashtable.c
 *
 *---------------------------------------------------------------------*/

static void 
pfs_free_pdce (void * val)
{
  pfs_mutex_lock (&((pdce_t *) val)->dir->lock);
  pfs_mutex_unlock (&((pdce_t *) val)->dir->lock);
  pfs_mutex_destroy (&((pdce_t *) val)->dir->lock);
  pfs_free_dir (((pdce_t *) val)->dir);
  free (val);
}



/* STATIC SERIALIZATION FUNCTION. */


/*---------------------------------------------------------------------
 * Method: pfs_(read|write)_(vv|ver|entry|dir)
 * Scope:  Static
 *
 * Serialization functions
 *
 *---------------------------------------------------------------------*/

static void
pfs_write_vv (int wd, 
	      struct pfs_vv * vv)
{
  int i;
  writen (wd, vv->last_updt, PFS_ID_LEN);
  writen (wd, &vv->len, sizeof (uint32_t));
  for (i = 0; i < vv->len; i ++) {
    writen (wd, vv->sd_id[i], PFS_ID_LEN);
    writen (wd, &vv->value[i], sizeof (uint32_t));
  }
}

static void 
pfs_write_ver (int wd, 
	       const struct pfs_ver * ver)
{
  writen (wd, &ver->type, sizeof (uint8_t));
  writen (wd, ver->dst_id, PFS_ID_LEN);
  pfs_write_vv (wd, ver->vv);
}

static void
pfs_write_entry (int wd,
		 struct pfs_entry * entry)
{
  int i;
  writen (wd, entry->name, PFS_NAME_LEN);
  writen (wd, &entry->main_idx, sizeof (uint32_t));
  writen (wd, &entry->ver_cnt, sizeof (uint32_t));
  for (i = 0; i < entry->ver_cnt; i ++) {
    pfs_write_ver (wd, entry->ver[i]);
  }
}

static void
pfs_write_dir (int wd,
	       struct pfs_dir * dir)
{
  int i;
  writen (wd, "DIR", 3);
  writen (wd, dir->id, PFS_ID_LEN);
  writen (wd, &dir->entry_cnt, sizeof (uint32_t));
  for (i = 0; i < dir->entry_cnt; i ++) {
    pfs_write_entry (wd, dir->entry[i]);
  }
}

static struct pfs_vv *
pfs_read_vv (int rd)
{
  int i;
  struct pfs_vv * vv = NULL;
  vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  ASSERT (vv != NULL);
  readn (rd, vv->last_updt, PFS_ID_LEN);
  readn (rd, &vv->len, sizeof (uint32_t));
  vv->sd_id = (char **) malloc (vv->len * sizeof (char *));
  vv->value = (uint32_t *) malloc (vv->len * sizeof (uint32_t));
  ASSERT (vv->sd_id != NULL && vv->value != NULL);
  for (i = 0; i < vv->len; i ++) {
    vv->sd_id[i] = (char *) malloc (PFS_ID_LEN * sizeof (char));
    ASSERT (vv->sd_id[i] != NULL);
    readn (rd, vv->sd_id[i], PFS_ID_LEN);
    readn (rd, &vv->value[i], sizeof (uint32_t));    
  }
  return vv;
}

static struct pfs_ver *
pfs_read_ver (int rd)
{
  struct pfs_ver * ver = NULL;
  ver = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
  ASSERT (ver != NULL);
  readn (rd, &ver->type, sizeof (uint8_t));
  readn (rd, ver->dst_id, PFS_ID_LEN);
  ver->vv = pfs_read_vv (rd);
  return ver;
}

static struct pfs_entry *
pfs_read_entry (int rd)
{
  int i;
  struct pfs_entry * entry = NULL;
  entry = (struct pfs_entry *) malloc (sizeof (struct pfs_entry));
  ASSERT (entry != NULL);
  readn (rd, entry->name, PFS_NAME_LEN);
  readn (rd, &entry->main_idx, sizeof (uint32_t));
  readn (rd, &entry->ver_cnt, sizeof (uint32_t));
  entry->ver = (struct pfs_ver **) malloc (entry->ver_cnt * sizeof (void *));
  ASSERT (entry->ver != NULL);
  for (i = 0; i < entry->ver_cnt; i ++) {
    entry->ver[i] = pfs_read_ver (rd);
  }
  return entry;
}

static struct pfs_dir *
pfs_read_dir (int rd)
{
  int i;
  struct pfs_dir * dir = NULL;
  dir = (struct pfs_dir *) malloc (sizeof (struct pfs_dir));
  ASSERT (dir != NULL);
  readn (rd, dir->type, 3);
  if (memcmp (dir->type, "DIR", 3) != 0) {
    free (dir);
    return NULL;
  }
  readn (rd, dir->id, PFS_ID_LEN);
  readn (rd, &dir->entry_cnt, sizeof (uint32_t));
  if (dir->entry_cnt > 0) {
    dir->entry = (struct pfs_entry **) malloc (dir->entry_cnt * sizeof (void *));
    ASSERT (dir->entry != NULL);
    for (i = 0; i < dir->entry_cnt; i ++) {
      dir->entry[i] = pfs_read_entry (rd);
    }
  }
  else
    dir->entry = NULL;
  return dir;
}
