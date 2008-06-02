#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <pthread.h>

#include "prop.h"
#include "pfsd.h"
#include "log.h"
#include "../libpfs/lib/io.h"
#include "../libpfs/group.h"
#include "../libpfs/pfs.h"
#include "../libpfs/entry.h"
#include "../libpfs/updt.h"
#include "../libpfs/entry.h"

struct prop_arg {
  char grp_id [PFS_ID_LEN];
  char sd_id [PFS_ID_LEN];
  int tun_port;
  struct prop_arg * next;
};


int
show_time (void)
{
  struct prop_arg * args = NULL;
  struct prop_arg * arg;
  struct pfsd_sd * sd;
  struct pfsd_sd * c_sd;
  
  struct pfs_group * pfs_grp;
  struct pfs_sd * pfs_sd;

  pfs_mutex_lock (&pfsd->sd_lock);
  pfs_mutex_lock (&pfsd->pfs->group_lock);
  
  /* We construct the list of sd,grp couple to update. */

  sd = pfsd->sd;
  while (sd != NULL) {
    c_sd = pfsd->sd;
    while (c_sd != NULL) {
      if (strncmp (sd->sd_id, c_sd->sd_id, PFS_ID_LEN) == 0 &&
	  c_sd->tun_conn < sd->tun_conn) {
	goto better_sd;
      }
      c_sd = c_sd->next;
    }

    pfs_grp = pfsd->pfs->group;
    
    while (pfs_grp != NULL) {
      pfs_sd = pfs_grp->sd;
      while (pfs_sd != NULL) {
	if (strncmp (pfs_sd->sd_id, sd->sd_id, PFS_ID_LEN) == 0) {
	  arg = (struct prop_arg *) malloc (sizeof (struct prop_arg));
	  strncpy (arg->sd_id, sd->sd_id, PFS_ID_LEN);
	  strncpy (arg->grp_id, pfs_grp->grp_id, PFS_ID_LEN);
	  arg->tun_port = sd->tun_port;
	  arg->next = args;
	  args = arg;
	}
	pfs_sd = pfs_sd->next;
      }
      pfs_grp = pfs_grp->next;
    }

  better_sd:
    sd = sd->next;
  }
  
  pfs_mutex_unlock (&pfsd->pfs->group_lock);
  pfs_mutex_unlock (&pfsd->sd_lock);

  /*
   * We don't spawn thread since anyway locking discipline
   * will order the processing. 
   */ 
  
  while (args != NULL) {
    arg = args;
    args = args->next;
    updater (arg);
  }

  return 0;
}


/*---------------------------------------------------------------------
 * Method: updater
 *
 * Handles propagation of updates for sd and group specified by
 * prop_args argument. Launched in separate thread. Tasks :
 * - get grp_status from that id
 * - update local state from it
 * - propagate unpropagated updates for that id based on its sv.
 *
 *---------------------------------------------------------------------*/

int
updater (struct prop_arg * prop_arg)
{
  char grp_id [PFS_ID_LEN];
  char sd_id [PFS_ID_LEN];
  int tun_port;
  char * in_buf;

  int tun_sd = -1;
  struct hostent *he;
  struct sockaddr_in tun_addr;

  struct pfs_updt * log;
  struct pfsd_grp_log * grp_log;

  struct pfs_vv * sd_sv = NULL;

  strncpy (grp_id, prop_arg->grp_id, PFS_ID_LEN);
  strncpy (sd_id, prop_arg->sd_id, PFS_ID_LEN);
  tun_port = prop_arg->tun_port;
  free (prop_arg);

#ifdef DEBUG
  printf ("STARTING UPDATER FOR : %.*s:%.*s\n",
	  PFS_ID_LEN, grp_id, PFS_ID_LEN, sd_id);
#endif
  
  if ((tun_sd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    goto error;
  if ((he = gethostbyname ("localhost")) == NULL)
    goto error;
  
  bzero (&tun_addr, sizeof (tun_addr));
  tun_addr.sin_family = AF_INET;
  tun_addr.sin_port = htons (tun_port);
  tun_addr.sin_addr = *((struct in_addr *) he->h_addr);
  
  if (connect(tun_sd, (struct sockaddr *)&tun_addr,
	      sizeof (tun_addr)) < 0)
    goto error;
  
  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;
  if (strcmp (in_buf, OK) != 0) {
    if (strcmp (in_buf, "FAIL") == 0) {
      free (in_buf);
#ifdef DEBUG
      printf ("SD OFFLINE\n");
#endif
      goto done;
    }
    free (in_buf);
    goto error;
  }
  free (in_buf);

  /* Update local status with sd grp_stat. */

  if (update_status (tun_sd, grp_id, sd_id) != 0)
    goto error;
  
  
  /* Push the non-propagated updates. */

#ifdef DEBUG
  printf ("PUSHING UPDATES for %.*s:%.*s...\n",
	  PFS_ID_LEN, grp_id, PFS_ID_LEN, sd_id);
#endif
  
  sd_sv = pfs_group_get_sv (pfsd->pfs, grp_id, sd_id);
  if (sd_sv == NULL)
    goto error;

  pfs_mutex_lock (&pfsd->log_lock);

  grp_log = pfsd->log->grp_log;
  while (grp_log != NULL) {
    if (strncmp (grp_id, grp_log->grp_id, PFS_ID_LEN) == 0)
      goto found_grp;
    grp_log = grp_log->next;
  }
  
  printf ("grp not found\n");
  pfs_mutex_unlock (&pfsd->log_lock);
  goto done;
  
 found_grp:
  printf ("grp found\n");
  log = grp_log->log;
  
  while (log != NULL) {
    if (pfs_vv_cmp (sd_sv, log->ver->mv) <= 0)
      {
	if (net_prop_updt (tun_sd, grp_id, sd_id, log) != 0) {
	  pfs_mutex_unlock (&pfsd->log_lock);
	  goto error;
	}	
	pfs_group_updt_sv (pfsd->pfs, grp_id, sd_id, log->ver->mv);
      }
    log = log->next;
  }
  
  pfs_mutex_unlock (&pfsd->log_lock);
  
 done:
  if (sd_sv != NULL)
    pfs_free_vv (sd_sv);
  writeline (tun_sd, CLOSE, strlen (CLOSE));
  close(tun_sd);
  return 0;

 error:
#ifdef DEBUG
  printf ("ERROR DURING TRANSFER WITH %.*s:%.*s...\n",
	  PFS_ID_LEN, grp_id, PFS_ID_LEN, sd_id);
#endif
  if (sd_sv != NULL)
    pfs_free_vv (sd_sv);
  writeline (tun_sd, CLOSE, strlen (CLOSE));
  close(tun_sd);
  return 0;
}



/*---------------------------------------------------------------------
 * Method: update_status
 *
 * - get grp_status from that id
 * - update local state from it
 *
 *---------------------------------------------------------------------*/
int
update_status (int tun_sd,
	       char * grp_id,
	       char * sd_id)
{
  int i,j;
  int updt_sd_cnt;
  char updt_sd_id [PFS_ID_LEN];
  char updt_sd_owner [PFS_NAME_LEN];
  char updt_sd_name [PFS_NAME_LEN];
  struct pfs_vv * updt_sv = NULL;
  char * in_buf;
  int cnt;

  printf ("UDPATING STATUS for %.*s:%.*s...\n",
	  PFS_ID_LEN, grp_id, PFS_ID_LEN, sd_id);
 
  writeline (tun_sd, GRP_STAT, strlen (GRP_STAT));
  writeline (tun_sd, grp_id, PFS_ID_LEN);
 
  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;
  if (strcmp (in_buf, OK) != 0) {
    free (in_buf);
    goto error;
  }
  free (in_buf);

  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;
  updt_sd_cnt = atoi (in_buf);
  free (in_buf);

  for (i = 0; i < updt_sd_cnt; i ++)
    {
      in_buf = readline (tun_sd);
      if (in_buf == NULL) goto error;
      strncpy (updt_sd_id, in_buf, PFS_ID_LEN);
      free (in_buf);

      memset (updt_sd_owner, 0, PFS_NAME_LEN);
      in_buf = readline (tun_sd);
      if (in_buf == NULL) goto error;
      strncpy (updt_sd_owner, in_buf, PFS_NAME_LEN);
      free (in_buf);

      memset (updt_sd_name, 0, PFS_NAME_LEN);
      in_buf = readline (tun_sd);
      if (in_buf == NULL) goto error;
      strncpy (updt_sd_name, in_buf, PFS_NAME_LEN);
      free (in_buf);

      in_buf = readline (tun_sd);
      if (in_buf == NULL) goto error;
      cnt = atoi (in_buf);
      free (in_buf);

      updt_sv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
      updt_sv->len = cnt;
      updt_sv->sd_id = (char **) malloc (cnt * sizeof (char *));
      updt_sv->value = (uint64_t *) malloc (cnt * sizeof (uint64_t));
      for (j = 0; j < cnt; j ++)
	updt_sv->sd_id[j] = (char *) malloc (PFS_ID_LEN);

      for (j = 0; j < cnt; j ++)
	{
	  in_buf = readline (tun_sd);
	  if (in_buf == NULL) goto error;
	  strncpy (updt_sv->sd_id[j], in_buf, PFS_ID_LEN);
	  free (in_buf);
	  
	  in_buf = readline (tun_sd);
	  if (in_buf == NULL) goto error;
	  updt_sv->value[j] = atol (in_buf);
	  free (in_buf);	  
	}
      
      pfs_group_add_sd (pfsd->pfs, grp_id, updt_sd_id,
			updt_sd_owner, updt_sd_name);
      pfs_group_updt_sv (pfsd->pfs, grp_id, updt_sd_id, updt_sv); 

      pfs_free_vv (updt_sv);
    }
  
  printf ("done\n");
  return 0;

 error:
  if (updt_sv != NULL)
    pfs_free_vv (updt_sv);
  return -1;
}



int 
net_prop_updt (int tun_sd,
	       char * grp_id,
	       char * sd_id,
	       struct pfs_updt * updt)
{
  char * in_buf;
  char out_buf [4096];
  char * file_path = NULL;
  struct stat st_buf;
  int fd = -1;
  int len;
  int b_left;
  int b_done;
  int b_tot;
  int ack;

  writeline (tun_sd, UPDT, strlen (UPDT));
  net_write_updt (tun_sd, updt);
  
  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;

  if (strcmp (in_buf, GET_DATA) == 0)
    {
      free (in_buf);
      file_path = pfs_mk_file_path (pfsd->pfs,
				    updt->ver->dst_id);
      
      if (stat (file_path, &st_buf) != 0)
	{
	  sprintf (out_buf, "%d", 3);
	  writeline (tun_sd, out_buf, strlen (out_buf));
	  writen (tun_sd, "DEL", 3);
	  printf ("sent %s : %d / %d\n", updt->name, 3, 3);
	  b_tot = 3;
	  in_buf = readline (tun_sd);
	  if (in_buf == NULL) goto error;
	  len = atoi (in_buf);
	  free (in_buf);
	  printf ("acked %s : %d / %d\n", updt->name, 3, b_tot);
	}
      else
	{
	  sprintf (out_buf, "%d", (int) st_buf.st_size);
	  writeline (tun_sd, out_buf, strlen (out_buf));
	  
	  if ((fd = open (file_path, O_RDONLY)) < 0)
	    goto error;
	  
	  free (file_path);
	  file_path = NULL;
	  
	  b_left = (int) st_buf.st_size;
	  b_tot = (int) st_buf.st_size;
	  b_done = 0;
	  len = 1;
	  while (b_left > 0 && len > 0)
	    {
	      len = readn (fd, out_buf, ((b_left > 4096) ? 4096 : b_left));
	      writen (tun_sd, out_buf, len);
	      b_done += len;
	      b_left -= len;
	      printf ("sent %s : %d / %d\n", updt->name, b_done, b_tot);
	      ack = -1;
	      while (ack != 0 && ack != b_done) {
		in_buf = readline (tun_sd);
		if (in_buf == NULL) goto error;
		ack = atoi (in_buf);
		free (in_buf);
		printf ("acked %s : %d / %d\n", updt->name, ack, b_tot);
	      }
	    }
	  close (fd);
	  if (b_left != 0) {
	    goto error;
	  }
	}
   
      in_buf = readline (tun_sd);
      if (in_buf == NULL) goto error;
      if (strcmp (in_buf, OK) != 0) {
	free (in_buf);
	goto error;
      }
      free (in_buf);
    }
  else if (strcmp (in_buf, OK) == 0)
    {
      free (in_buf);
    }
  else 
    {
      free (in_buf);
      goto error;
    }
  
  return 0;

 error:
  if (file_path != NULL)
    free (file_path);
  return -1;
}


int 
net_write_updt (int tun_sd,
		struct pfs_updt * updt)
{
  char out_buf [512];
  int i;
  
  writeline (tun_sd, updt->grp_id, PFS_ID_LEN);
  writeline (tun_sd, updt->dir_id, PFS_ID_LEN);
  writeline (tun_sd, updt->name, strlen (updt->name));
  sprintf (out_buf, "%d", (int) updt->reclaim);
  writeline (tun_sd, out_buf, strlen (out_buf));

  sprintf (out_buf, "%d", (int) updt->ver->type);
  writeline (tun_sd, out_buf, strlen (out_buf));
  sprintf (out_buf, "%d", (int) updt->ver->st_mode);
  writeline (tun_sd, out_buf, strlen (out_buf));
  writeline (tun_sd, updt->ver->dst_id, PFS_ID_LEN);
  writeline (tun_sd, updt->ver->last_updt, PFS_ID_LEN);
  writeline (tun_sd, updt->ver->sd_orig, PFS_ID_LEN);
  sprintf (out_buf, "%llu", updt->ver->cs);
  writeline (tun_sd, out_buf, strlen (out_buf));

  sprintf (out_buf, "%d", updt->ver->mv->len);
  writeline (tun_sd, out_buf, strlen (out_buf));
  
  for (i = 0; i < updt->ver->mv->len; i ++)
    {
      writeline (tun_sd, updt->ver->mv->sd_id[i], PFS_ID_LEN);
      sprintf (out_buf, "%llu", updt->ver->mv->value[i]);
      writeline (tun_sd, out_buf, strlen (out_buf));
    }  

  return 0;
}
		
