/*
 * Directory, entry, version management
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

#include "entry.h"
#include "file.h"
#include "updt.h"
#include "dir_cache.h"
#include "group.h"

/*---------------------------------------------------------------------
 * Method: pfs_create_dir
 * Scope:  Global
 *
 * Create a new dir file and returns its newly created uid.
 *
 *---------------------------------------------------------------------*/

int
pfs_create_dir (struct pfs_instance * pfs, 
		char * dir_id)
{
  return pfs_create_dir_cache (pfs, dir_id);
}

/*---------------------------------------------------------------------
 * Method: pfs_create_dir_wit_id
 * Scope:  Global
 *
 * Create a new dir file with given id.
 *
 *---------------------------------------------------------------------*/

int
pfs_create_dir_with_id (struct pfs_instance * pfs, 
			const char * dir_id)
{
  return pfs_create_dir_cache_with_id (pfs, dir_id);
}

/*---------------------------------------------------------------------
 * Method: pfs_dir_rmdir
 * Scope:  Global
 *
 * Delete the directory file after checking atomically if dir is empty
 * Returns -errno code or 0 if successful
 *
 *---------------------------------------------------------------------*/

int pfs_dir_rmdir (struct pfs_instance *pfs,
		   const char * dir_id)
{
  struct pfs_dir * dir;
  int i, j;

  dir = pfs_get_dir_cache (pfs, dir_id);
  if (dir == NULL)
    return -ENOENT;

  for (i = 0; i < dir->entry_cnt; i ++) {
    for (j = 0; j < dir->entry[i]->ver_cnt; j ++) {
      if (dir->entry[i]->ver[j]->type != PFS_DEL) {
	pfs_unlock_dir_cache (pfs, dir);
	return -ENOTEMPTY;
      }
    }
  }
  pfs_unlock_dir_cache (pfs, dir);

  return pfs_remove_dir_cache (pfs, dir_id);
}

/*---------------------------------------------------------------------
 * Method: pfs_dir_empty
 * Scope:  Global
 *
 * Return 1 if dir empty 0 otherwise.
 *
 *---------------------------------------------------------------------*/

uint8_t 
pfs_dir_empty (struct pfs_instance *pfs,
	       char * dir_id)
{
  struct pfs_dir * dir;
  int i, j;
  
  dir = pfs_get_dir_cache (pfs, dir_id);  
  if (dir == NULL) {
    return 1;
  }

  for (i = 0; i < dir->entry_cnt; i ++) {
    for (j = 0; j < dir->entry[i]->ver_cnt; j ++) {
      if (dir->entry[i]->ver[j]->type != PFS_DEL) {
	pfs_unlock_dir_cache (pfs, dir);
	return 0;
      }
    }
  }

  pfs_unlock_dir_cache (pfs, dir);
  return 1;
}


/*---------------------------------------------------------------------
 * Method: pfs_set_entry
 * Scope:  Global
 *
 * Set entry version ver in dir_id with name. 
 * Synchonize using flocks on the file representing the
 * directory. Erase any entry dominated by this entry.
 * if reclaim : Reclaim Overriden ressources SML,FIL and DIR if empty
 * errval : -1 dir_id does not exist or is a file
 * errval : -2 ver dominated
 * errval : -3 ver dominate non empty dir
 * errval : -4 other error
 *
 * EVERYTHING HAPPENS HERE
 *
 *---------------------------------------------------------------------*/

int
pfs_set_entry (struct pfs_instance * pfs,
	       const char * grp_id,
	       const char * dir_id,
	       const char * name,
	       const uint8_t reclaim,
	       const struct pfs_ver * ver,
	       const uint8_t log)
{
  int m_upt_num, m_sd_upt_num, upt_num, sd_upt_num;
  int ver_cnt;
  int cmp_val;
  int trans_main_idx;
  int entry_idx;
  int i, j;
  struct pfs_ver * replay_ver = NULL;
  struct pfs_entry * new_entry = NULL;
  struct pfs_entry * entry = NULL;
  struct pfs_dir * dir;
  int retval = -4;
  struct pfs_vv * sv;

  /* get struct pfs_dir. */
  dir = pfs_get_dir_cache (pfs, dir_id);
  if (dir == NULL) {
    pfs_create_dir_cache_with_id (pfs, dir_id);
    dir = pfs_get_dir_cache (pfs, dir_id);
    if (dir == NULL) {    
      return -1;
    }
  }
  
  /* Lookup for this entry. */
  entry_idx = -1;
  for (i = 0; i < dir->entry_cnt; i ++) {
    if (strcmp (name, dir->entry[i]->name) == 0) {
      entry = dir->entry[i];
      entry_idx = i;
    }
  }

  if (entry != NULL) {
#ifdef DEBUG
    printf ("*** PFS_SET_ENTRY %s %d 0x%x", name, (int) ver->type, ver->st_mode);
    printf (" mv : ");
    pfs_print_vv (ver->mv);
    printf (" last_updt : %.2s cs : %d\n", ver->sd_orig, (int) ver->cs);
#endif
  }
  
  /* We check if it is actually a new version. */
  if (entry == NULL)
    {
      sv = pfs_group_get_sv (pfs, grp_id, pfs->sd_id);
      if (sv == NULL)
	goto error;
#ifdef DEBUG
      printf ("*** PFS_SET_ENTRY %s %d 0x%x", name, (int) ver->type, ver->st_mode);
      printf (" sv : ");
      pfs_print_vv (sv);
      printf (" mv : ");
      pfs_print_vv (ver->mv);
      printf (" last_updt : %.2s cs : %d\n", ver->sd_orig, (int) ver->cs);
#endif
      cmp_val = pfs_vv_cmp (ver->mv, sv);
      if (cmp_val <= 0) {
	for (i = 0; i < sv->len; i ++) {
	  if (strncmp (sv->sd_id[i], ver->sd_orig, PFS_ID_LEN) == 0) {
	    if (ver->cs <= sv->value[i]) {
	      printf ("*** PFS_SET_ENTRY : IGNORING ENTRY cs : %d sv : %d\n", (int) ver->cs, (int) sv->value[i]);
	      pfs_free_vv (sv);
	      goto done_no_updt;
	    }
	  }
	}
      }
      pfs_free_vv (sv);
    }

  /* Count new ver_cnt. */
  if (ver->type == PFS_DEL)
    ver_cnt = 0;
  else
    ver_cnt = 1;
  if (entry != NULL) {
    for (i = 0; i < entry->ver_cnt; i ++) {
      cmp_val = pfs_vv_cmp (ver->mv, entry->ver[i]->mv);
      if (cmp_val == 0) {
	if (ver->type == PFS_DEL) {
	  /*
	   * we have to replay the insertion so that the update
	   * is not ignored when retransmitted due to old cs
	   */
	  replay_ver = pfs_cpy_ver (entry->ver[i]);
	  pfs_unlock_dir_cache (pfs, dir);
	  /* restart */
	  pfs_reset_gen_vv (pfs, replay_ver);
	  if ((i = pfs_set_entry (pfs, grp_id, 
				  dir_id, name, 
				  0, replay_ver, 1)) < 0) {
	    pfs_free_ver (replay_ver);
	    return i;
	  }
	  pfs_free_ver (replay_ver);
	  return 0;
	}
	ver_cnt ++;
      }
      if (cmp_val == -1) {
	retval = -2;
	goto error;
      }
      if (cmp_val == 1 && reclaim) {
	if (entry->ver[i]->type == PFS_DIR) {
	  /*
	   * Nothing to do cf. replay in next loop
	   */
	}
      }
      if (cmp_val == 2) {
	goto done_no_updt;
      }
    }
  }
  
  /* 
   * OKAY ! we commit the new entry. 
   * From now on it's goto done if we're done. 
   */

  /* We cleanup back storage data */
  j = 0;
  for (i = 0; entry != NULL && i < entry->ver_cnt; i ++) {
    cmp_val = pfs_vv_cmp (ver->mv, entry->ver[i]->mv);
    if (cmp_val == 1 && reclaim) {
      switch (entry->ver[i]->type)
	{
	case PFS_DIR:
	  {
	    retval = pfs_dir_rmdir (pfs, entry->ver[i]->dst_id);
	    /*
	     * Failed Reclamation will only happen in the case of network
	     * updates. We increase our DIR and retart the two set_entry.
	     */
	    if (retval == -ENOTEMPTY) {
	      replay_ver = pfs_cpy_ver (entry->ver[i]);
	      pfs_unlock_dir_cache (pfs, dir);
	      /* restart */
	      pfs_reset_gen_vv (pfs, replay_ver);
	      if ((i = pfs_set_entry (pfs, grp_id, 
				      dir_id, name, 
				      0, replay_ver, 1)) < 0) {
		pfs_free_ver (replay_ver);
		return i;
	      }
	      pfs_free_ver (replay_ver);
	      if ((i = pfs_set_entry (pfs, grp_id, 
				      dir_id, name, 
				      reclaim, ver, 1)) < 0)
		return i;
	      return 0;
	    }
	  }
	  break;	  
	case PFS_FIL:
	  pfs_file_unlink (pfs, entry->ver[i]->dst_id);
	  break;
	case PFS_SML:
	  pfs_file_unlink (pfs, entry->ver[i]->dst_id);
	  break;
	}
    }
  }


  /* The file is to be deleted. */
  if (ver_cnt == 0) {
    if (entry == NULL) {
      goto done;
    }    
    dir->entry_cnt --;
    for (i = entry_idx; i < dir->entry_cnt; i ++) {
      dir->entry[i] = dir->entry[i+1];
    }
    pfs_free_entry (entry);
    dir->entry = (struct pfs_entry **) realloc (dir->entry, (dir->entry_cnt) * sizeof (void *));
    goto done;
  }


  /* Create new_entry. */
  new_entry = (struct pfs_entry *) malloc (sizeof (struct pfs_entry));
  strncpy (new_entry->name, name, PFS_NAME_LEN);
  new_entry->ver_cnt = ver_cnt;
  new_entry->ver = (struct pfs_ver **) malloc (ver_cnt * sizeof (void *));
  for (i = 0; i < new_entry->ver_cnt; i ++)
    new_entry->ver[i] = NULL;

  
  /* Populate new_entry with copy vers. */
  j = 0;
  trans_main_idx = -1; 
  for (i = 0; entry != NULL && i < entry->ver_cnt; i ++) {
    cmp_val = pfs_vv_cmp (ver->mv, entry->ver[i]->mv);
    if (cmp_val == 0) {
      /* translated main_idx for new_entry. */
      if (i == entry->main_idx)
	trans_main_idx = j;
      new_entry->ver[j] = pfs_cpy_ver (entry->ver[i]);
      j ++;
    }
  }
  if (ver->type != PFS_DEL) {
    new_entry->ver[j] = pfs_cpy_ver (ver); 
    ASSERT (j == new_entry->ver_cnt - 1);
  }


  /* Determine new_entry->main_idx. */
  if (entry == NULL) {
    new_entry->main_idx = 0;
  }
  else {
    m_upt_num = -1; m_sd_upt_num = -1; upt_num = -1; sd_upt_num = -1;
    
    for (i = 0; i < entry->ver[entry->main_idx]->mv->len; i ++) {
      m_upt_num += entry->ver[entry->main_idx]->mv->value[i];
      if (memcmp (entry->ver[entry->main_idx]->mv->sd_id[i],
		  pfs->sd_id, PFS_ID_LEN) == 0) {
	m_sd_upt_num = entry->ver[entry->main_idx]->mv->value[i];
      }
    }
    for (i = 0; i < ver->mv->len; i ++) {
      upt_num += ver->mv->value[i];
      if (memcmp (ver->mv->sd_id[i],
		  pfs->sd_id, PFS_ID_LEN) == 0) {
	sd_upt_num = ver->mv->value[i];
      }
    }
    if (sd_upt_num > m_sd_upt_num)
      new_entry->main_idx = new_entry->ver_cnt - 1;
    if (sd_upt_num == m_sd_upt_num && upt_num >= m_upt_num)
      new_entry->main_idx = new_entry->ver_cnt - 1;
    if (sd_upt_num == m_sd_upt_num && upt_num < m_upt_num) {
      ASSERT (trans_main_idx != -1);
      new_entry->main_idx = trans_main_idx;
    }
    if (sd_upt_num < m_sd_upt_num) {
      ASSERT (trans_main_idx != -1);
      new_entry->main_idx = trans_main_idx;
    }
  }

  /* Insert new_entry in dir. */
  if (entry != NULL) {
    ASSERT (entry_idx != -1);
    dir->entry[entry_idx] = new_entry;
    pfs_free_entry (entry);
  }
  else {
    dir->entry = (struct pfs_entry **) realloc (dir->entry, (dir->entry_cnt + 1) * sizeof (void *));
    dir->entry [dir->entry_cnt] = new_entry;
    dir->entry_cnt ++;     
  }
 
 done:
  /* log the set_entry. */
  if (log) {
    if (pfs_push_updt (pfs, grp_id, dir_id, name, reclaim, ver) != 0)
      goto error;
  }
  /* Update sync vector. */
  if (pfs_group_updt_sv (pfs, grp_id, pfs->sd_id, ver->mv) != 0)
    goto error;
  pfs_unlock_dir_cache (pfs, dir);
  pfs_dirty_dir_cache (pfs, dir_id);
  return 0;
  
done_no_updt:
  /* Update sync vector. */
  if (pfs_group_updt_sv (pfs, grp_id, pfs->sd_id, ver->mv) != 0)
    goto error;
  pfs_unlock_dir_cache (pfs, dir);
  return 0;

 error:
  pfs_unlock_dir_cache (pfs, dir);
  return retval;
}



/*---------------------------------------------------------------------
 * Method: pfs_get_entry
 * Scope:  Global
 *
 * Get entry in dir_id with name. 
 * Synchonize using flocks on the file representing the
 * directory. Erase any entr dominated by this entry.
 *
 *---------------------------------------------------------------------*/

struct pfs_entry * 
pfs_get_entry (struct pfs_instance * pfs,
	       char * dir_id,
	       char * name)
{
  struct pfs_entry * entry = NULL;
  struct pfs_dir * dir;
  int i;

  dir = pfs_get_dir_cache (pfs, dir_id);
  if (dir == NULL) {
    pfs_unlock_dir_cache (pfs, dir);
    return NULL;
  }
  
  for (i = 0; i < dir->entry_cnt; i ++) {
    if (strncmp (name, dir->entry[i]->name, PFS_NAME_LEN) == 0) {
      entry = pfs_cpy_entry (dir->entry[i]);
      break;
    }
  }
  pfs_unlock_dir_cache (pfs, dir);
  return entry;
}



/*---------------------------------------------------------------------
 * Method: pfs_print_entry
 * Scope:  Global
 *
 * Prints this entry versions
 *
 *---------------------------------------------------------------------*/

void pfs_print_entry (struct pfs_instance * pfs,
		      char * dir_id,
		      char * name)
{
  int i,j;
  char id [PFS_ID_LEN + 1];
  struct pfs_entry * entry = pfs_get_entry (pfs, dir_id, name);
  printf ("%s - ", entry->name);
  printf ("main_idx : %d\n", entry->main_idx);
  for (i = 0; i < entry->ver_cnt; i ++) {
    printf ("%d- ", i);
    switch (entry->ver[i]->type) {
    case PFS_DIR:
      printf ("DIR ");
      break;
    case PFS_FIL:
      printf ("FIL ");
      break;
    case PFS_SML:
      printf ("SML ");
      break;
    case PFS_DEL:
      printf ("DEL ");
      break;
    }
    sprintf (id, "%s", entry->ver[i]->dst_id);
    printf ("%s ", id);
    printf ("last_updt : %s\n", entry->ver[i]->last_updt);
    for (j = 0; j < entry->ver[i]->mv->len; j ++) {
      sprintf (id, "%s", entry->ver[i]->mv->sd_id[j]);
      printf ("  %s : %d\n", id, (int) entry->ver[i]->mv->value[j]);
    }
  }
  pfs_free_entry (entry);
}


/*---------------------------------------------------------------------
 * Method: pfs_vv_cmp
 * Scope:  Global
 *
 * Compares two version vectors. 
 * Returns 1 if a dominates b -1 if b dominates a 0 otherwise
 * 2 if equals
 *
 *---------------------------------------------------------------------*/

int
pfs_vv_cmp (struct pfs_vv * a,
	    struct pfs_vv * b)
{
  int i, j;
  uint8_t found = 0;
  int a_dom_b = 1;
  int b_dom_a = 1;

  for (i = 0; i < b->len; i ++) {
    found = 0;
    for (j = 0; j < a->len; j ++) {
      if (memcmp (b->sd_id[i], a->sd_id[j], PFS_ID_LEN) == 0) {
	found = 1;
	if (b->value[i] > a->value[j]) {
	  a_dom_b = 0;
	  goto done_ab;
	}
	else break;
      }
    }
    if (!found) {
      a_dom_b = 0;
      goto done_ab;
    }
  }
 done_ab:
  
  for (i = 0; i < a->len; i ++) {
    found = 0;
    for (j = 0; j < b->len; j ++) {
      if (memcmp (a->sd_id[i], b->sd_id[j], PFS_ID_LEN) == 0) {
	found = 1;
	if (a->value[i] > b->value[j]) {
	  b_dom_a = 0;
	  goto done_ba;
	}
	else break;
      }
    }
    if (!found) {
      b_dom_a = 0;
      goto done_ba;
    }
  }
 done_ba:
  
  /* Happens if same vv. Happens on replica coming from the net. */
  if (a_dom_b * b_dom_a != 0)
    return 2;
  if (a_dom_b)
    return 1;
  if (b_dom_a)
    return -1;
  return 0;
}


/*---------------------------------------------------------------------
 * Method: pfs_vv_merge
 * Scope:  Global
 *
 * merge two version vectors and increments the version of the acutal
 * sd. Suppose version values are ordered by sd_id using
 * strncmp. Version vectors of length > 1 are supposed to be created
 * through this functions to maintain ordering. Set last_updater as the 
 * local_sd
 *
 *---------------------------------------------------------------------*/

struct pfs_vv * pfs_vv_merge (struct pfs_instance * pfs,
			      const struct pfs_vv * a,
			      const struct pfs_vv * b)
{
  int a_idx = 0;
  int b_idx = 0;
  int i;
  int cmp_val;
  struct pfs_vv * new_vv = NULL;
  
  new_vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  ASSERT (new_vv != NULL);
  new_vv->len = 0;

  /* Calculate the new len. */
  while (a_idx < a->len && b_idx < b->len) {
    cmp_val = strncmp (a->sd_id[a_idx], b->sd_id[b_idx], PFS_ID_LEN);
    if (cmp_val == 0) {
      new_vv->len ++;
      a_idx ++;
      b_idx ++;
    }
    if (cmp_val < 0) {
      new_vv->len ++;
      a_idx ++;
    }
    if (cmp_val > 0) {
      new_vv->len ++;
      b_idx ++;
    }
  }
  while (a_idx < a->len) {
    new_vv->len ++;
    a_idx ++;
  }
  
  while (b_idx < b->len) {
    new_vv->len ++;
    b_idx ++;
  }

  new_vv->sd_id = (char **) malloc (sizeof (void *) * new_vv->len);
  new_vv->value = (uint64_t *) malloc (sizeof (uint64_t) * new_vv->len);
  ASSERT (new_vv->sd_id != NULL && new_vv->value != NULL);
  for (i = 0; i < new_vv->len; i ++) {
    new_vv->sd_id[i] = (char *) malloc (PFS_ID_LEN);
    ASSERT (new_vv->sd_id[i] != NULL);
  }

  i = 0;
  a_idx = 0;
  b_idx = 0;
  /* Create the new_vv. */
  while (a_idx < a->len && b_idx < b->len) {
    cmp_val = strncmp (a->sd_id[a_idx], b->sd_id[b_idx], PFS_ID_LEN);
    if (cmp_val == 0) {
      memcpy (new_vv->sd_id[i], a->sd_id[a_idx], PFS_ID_LEN);
      new_vv->value[i] = (a->value[a_idx] > b->value[b_idx]) ? a->value[a_idx] : b->value[b_idx];
      i ++;
      a_idx ++;
      b_idx ++;
    }
    if (cmp_val < 0) {
      memcpy (new_vv->sd_id[i], a->sd_id[a_idx], PFS_ID_LEN);
      new_vv->value[i] = a->value[a_idx];
      i ++;
      a_idx ++;
    }
    if (cmp_val > 0) {
      memcpy (new_vv->sd_id[i], b->sd_id[b_idx], PFS_ID_LEN);
      new_vv->value[i] = b->value[b_idx];
      i ++;
      b_idx ++;
    }
  }
  while (a_idx < a->len) {
    memcpy (new_vv->sd_id[i], a->sd_id[a_idx], PFS_ID_LEN);
    new_vv->value[i] = a->value[a_idx];
    i ++;
    a_idx ++;
  }
  
  while (b_idx < b->len) {
    memcpy (new_vv->sd_id[i], b->sd_id[b_idx], PFS_ID_LEN);
    new_vv->value[i] = b->value[b_idx];
    i ++;
    b_idx ++;
  }

  return new_vv;
}


/*---------------------------------------------------------------------
 * Method: pfs_vv_incr
 * Scope:  Global
 *
 * Increments the vv value for the local sd. Add the sd entry to vv
 * if it is not present yet.
 *
 *---------------------------------------------------------------------*/

int pfs_vv_incr (struct pfs_instance * pfs,
		 struct pfs_ver * ver)
{
  int i,j;
  int cmp_val;
  
  for (i = 0; i < ver->mv->len; i ++) {
    cmp_val = strncmp (ver->mv->sd_id[i], pfs->sd_id, PFS_ID_LEN);
    if (cmp_val == 0) {
      ver->mv->value[i] = pfs_incr_updt_cnt (pfs);
      strncpy (ver->last_updt, pfs->sd_id, PFS_ID_LEN);
      return 0;
    }
  }
  
  ver->mv->len ++;
  ver->mv->sd_id = (char **) realloc (ver->mv->sd_id, sizeof (char *) * ver->mv->len);
  ver->mv->value = (uint64_t *) realloc (ver->mv->value, sizeof (uint64_t) * ver->mv->len);
  ASSERT (ver->mv->sd_id != NULL && ver->mv->value != NULL);
  
  for (i = 0; i < ver->mv->len - 1; i ++) {
    cmp_val = strncmp (ver->mv->sd_id[i], pfs->sd_id, PFS_ID_LEN);
    ASSERT (cmp_val != 0);
    if (cmp_val > 0)
      break;
  }
  
  for (j = ver->mv->len - 2; j >= i; j --) {
    ver->mv->sd_id[j+1] = ver->mv->sd_id[j];
    ver->mv->value[j+1] = ver->mv->value[j];
  }
  
  ver->mv->sd_id [i] = (char *) malloc (PFS_ID_LEN * sizeof (char));
  ASSERT (ver->mv->sd_id[i] != NULL);
  memcpy (ver->mv->sd_id[i], pfs->sd_id, PFS_ID_LEN);
  ver->mv->value[i] = pfs_incr_updt_cnt (pfs);
  strncpy (ver->last_updt, pfs->sd_id, PFS_ID_LEN);
  
  return 0;
}

int
pfs_reset_gen_vv (struct pfs_instance * pfs,
		  struct pfs_ver * ver)
{
  uint64_t cs;
  pfs_vv_incr (pfs, ver);
  pfs_mutex_lock (&pfs->info_lock);
  cs = pfs->updt_cnt;
  pfs_mutex_unlock (&pfs->info_lock);
  strncpy (ver->sd_orig, pfs->sd_id, PFS_ID_LEN);
  ver->cs = cs;
  return 0;
}


int
pfs_gen_vv (struct pfs_instance * pfs,
	    struct pfs_ver * ver)
{
  ver->mv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  strncpy (ver->last_updt, pfs->sd_id, PFS_ID_LEN);
  ver->mv->len = 1;
  ver->mv->sd_id = (char **) malloc (sizeof (char *));
  ver->mv->sd_id[0] = (char *) malloc (PFS_ID_LEN);
  strncpy (ver->mv->sd_id[0], pfs->sd_id, PFS_ID_LEN);
  ver->mv->value = (uint64_t *) malloc (sizeof (uint64_t));
  ver->mv->value[0] = pfs_incr_updt_cnt (pfs);
  strncpy (ver->sd_orig, pfs->sd_id, PFS_ID_LEN);
  ver->cs = ver->mv->value[0];
  
  return 0;
}

/* DEALLOC/CPY FUNCTION. */


/*---------------------------------------------------------------------
 * Method: pfs_(cpy|free)_(vv|ver|entry|dir)
 * Scope:  Global
 *
 * Dealloc/Cpy helper functions
 *
 *---------------------------------------------------------------------*/

struct pfs_vv * 
pfs_cpy_vv (struct pfs_vv * vv)
{
  int i;
  struct pfs_vv * vv_cpy = NULL;
  vv_cpy = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  ASSERT (vv_cpy != NULL);
  vv_cpy->len = vv->len;
  vv_cpy->sd_id = (char **) malloc (vv_cpy->len * sizeof (char *));
  vv_cpy->value = (uint64_t *) malloc (vv_cpy->len * sizeof (uint64_t));
  ASSERT (vv_cpy->sd_id != NULL && vv_cpy->value != NULL);
  for (i = 0; i < vv_cpy->len; i ++) {
    vv_cpy->sd_id[i] = (char *) malloc (PFS_ID_LEN * sizeof (char));
    ASSERT (vv_cpy->sd_id[i] != NULL);
    memcpy (vv_cpy->sd_id[i], vv->sd_id[i], PFS_ID_LEN);
    vv_cpy->value[i] = vv->value[i];
  }
  return vv_cpy;
}

struct pfs_ver *
pfs_cpy_ver (const struct pfs_ver * ver)
{
  struct pfs_ver * ver_cpy = NULL;
  ver_cpy = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
  ASSERT (ver_cpy != NULL);
  ver_cpy->type = ver->type;
  ver_cpy->st_mode = ver->st_mode;
  memcpy (ver_cpy->dst_id, ver->dst_id, PFS_ID_LEN);
  memcpy (ver_cpy->last_updt, ver->last_updt, PFS_ID_LEN);
  memcpy (ver_cpy->sd_orig, ver->sd_orig, PFS_ID_LEN);
  ver_cpy->cs = ver->cs;
  ver_cpy->mv = pfs_cpy_vv (ver->mv);
  return ver_cpy;
}

struct pfs_entry *
pfs_cpy_entry (struct pfs_entry * entry)
{
  int i;
  struct pfs_entry * entry_cpy = NULL;
  entry_cpy = (struct pfs_entry *) malloc (sizeof (struct pfs_entry));
  ASSERT (entry_cpy != NULL);
  memcpy (entry_cpy->name, entry->name, PFS_NAME_LEN);
  entry_cpy->main_idx = entry->main_idx;
  entry_cpy->ver_cnt = entry->ver_cnt;
  entry_cpy->ver = (struct pfs_ver **) malloc (entry_cpy->ver_cnt * sizeof (void *));
  ASSERT (entry_cpy->ver != NULL);
  for (i = 0; i < entry_cpy->ver_cnt; i ++) {
    entry_cpy->ver[i] = pfs_cpy_ver (entry->ver[i]);
  }
  return entry_cpy;
}


void 
pfs_free_vv (struct pfs_vv * vv)
{
  int i;
  for (i = 0; i < vv->len; i ++)
    free (vv->sd_id[i]);
  free (vv->sd_id);
  free (vv->value);
  free (vv);
}


void
pfs_free_ver (struct pfs_ver * ver)
{
  pfs_free_vv (ver->mv);
  free (ver);
}

void
pfs_free_entry (struct pfs_entry * entry)
{
  int i;
  for (i = 0; i < entry->ver_cnt; i ++)
    pfs_free_ver (entry->ver[i]);
  free (entry->ver);
  free (entry);
}

void
pfs_free_dir (struct pfs_dir * dir)
{
  int i;
  for (i = 0; i < dir->entry_cnt; i ++)
    pfs_free_entry (dir->entry[i]);
  free (dir->entry);
  free (dir);
}


void
pfs_write_vv (int wd, 
	      struct pfs_vv * vv)
{
  int i;
  writen (wd, &vv->len, sizeof (uint32_t));
  for (i = 0; i < vv->len; i ++) {
    writen (wd, vv->sd_id[i], PFS_ID_LEN);
    writen (wd, &vv->value[i], sizeof (uint64_t));
  }
}

struct pfs_vv *
pfs_read_vv (int rd)
{
  int i;
  struct pfs_vv * vv = NULL;
  vv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  ASSERT (vv != NULL);
  readn (rd, &vv->len, sizeof (uint32_t));
  vv->sd_id = (char **) malloc (vv->len * sizeof (char *));
  vv->value = (uint64_t *) malloc (vv->len * sizeof (uint64_t));
  ASSERT (vv->sd_id != NULL && vv->value != NULL);
  for (i = 0; i < vv->len; i ++) {
    vv->sd_id[i] = (char *) malloc (PFS_ID_LEN);
    ASSERT (vv->sd_id[i] != NULL);
    readn (rd, vv->sd_id[i], PFS_ID_LEN);
    readn (rd, &vv->value[i], sizeof (uint64_t));    
  }
  return vv;
}


int
pfs_print_vv (struct pfs_vv * vv)
{
  int j;
  printf ("<");
  for (j = 0; j < vv->len; j ++) {
    printf ("%.2s:%d", vv->sd_id[j], (int) vv->value[j]);
    if (j < vv->len - 1)
      printf (", ");
  }
  printf (">");
  return 0;
}


struct pfs_ver *
pfs_read_ver (int rd)
{
  struct pfs_ver * ver = NULL;
  ver = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
  ASSERT (ver != NULL);
  readn (rd, &ver->type, sizeof (uint8_t));
  readn (rd, &ver->st_mode, sizeof (mode_t));
  readn (rd, ver->dst_id, PFS_ID_LEN);
  readn (rd, ver->last_updt, PFS_ID_LEN);
  readn (rd, ver->sd_orig, PFS_ID_LEN);
  readn (rd, &ver->cs, sizeof (uint64_t));
  ver->mv = pfs_read_vv (rd);
  return ver;
}

void 
pfs_write_ver (int wd, 
	       const struct pfs_ver * ver)
{
  writen (wd, &ver->type, sizeof (uint8_t));
  writen (wd, &ver->st_mode, sizeof (mode_t));
  writen (wd, ver->dst_id, PFS_ID_LEN);
  writen (wd, ver->last_updt, PFS_ID_LEN);
  writen (wd, ver->sd_orig, PFS_ID_LEN);
  writen (wd, &ver->cs, sizeof (uint64_t));
  pfs_write_vv (wd, ver->mv);
}

