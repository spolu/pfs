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

#include "srv.h"
#include "../libpfs/lib/io.h"
#include "pfsd.h"
#include "../libpfs/group.h"
#include "../libpfs/pfs.h"
#include "../libpfs/entry.h"
#include "../libpfs/updt.h"
#include "../libpfs/entry.h"
#include "prop.h"

struct sd_arg
{
  int sd;
};

void *
start_srv (void * v)
{
  int srv_sd, cli_sd;
  struct sd_arg * arg;
  socklen_t len;
  struct sockaddr_in srv_addr, cli_addr;
  int yes = 1;
  pthread_t cli_thread;

 start:
  if ((srv_sd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
    perror ("socket error");
    exit (1);
  }

  if (setsockopt (srv_sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("setsockopt error");
    exit(1);
  }

  bzero (&srv_addr, sizeof (srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons (pfsd->pfsd_port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind (srv_sd, (struct sockaddr *) &srv_addr, sizeof srv_addr) < 0) {
    perror ("bind error");
    exit (1);
  }

  if (listen (srv_sd, LISTENQ) < 0) {
    perror ("listen error");
    exit (1);
  }

  for (;;) {
    len = sizeof (cli_addr);
    if ((cli_sd = accept (srv_sd, (struct sockaddr *) &cli_addr, &len)) < 0) {
      if (errno != EINTR) {
	close (srv_sd);
	goto start;
      }
      continue;
    }
    arg = (struct sd_arg *) malloc (sizeof (struct sd_arg));
    arg->sd = cli_sd;
    pthread_create (&cli_thread, NULL, handle_client, (void *) arg);
  }

  return 0;
}



void *
handle_client (void * sd_arg)
{
  int cli_sd;
  int ret;
  fd_set fdmask;
  struct timeval timeout;
  char * buf;

  cli_sd = ((struct sd_arg *) sd_arg)->sd;
  free (sd_arg);

  while (1) {

    /* Handle timeouts. */
    FD_ZERO(&fdmask);
    FD_SET(cli_sd, &fdmask);
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;
    ret = select (cli_sd + 1, &fdmask, NULL, NULL, &timeout);
    if (ret == 0)
      goto done;
    
    buf = readline (cli_sd);

    if (buf == NULL)
      goto error;

    if (strcmp (GRP_STAT, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", GRP_STAT);
#endif
      if (handle_grp_stat (cli_sd) != 0)
	goto error;
    }
    
    else if (strcmp (ONLINE, buf) == 0) {
      if (handle_online (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (OFFLINE, buf) == 0) {
      if (handle_offline (cli_sd) != 0)
	goto error;
    }
    
    else if (strcmp (UPDT, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", UPDT);
#endif
      if (handle_updt (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (ADD_SD, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", ADD_SD);
#endif
      if (handle_add_sd (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (ADD_GRP, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", ADD_GRP);
#endif
      if (handle_add_grp (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (LIST_SD, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", LIST_SD);
#endif
      if (handle_list_sd (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (CREAT_GRP, buf) == 0) {
#ifdef DEBUG
      printf ("*** HANDLE_CLIENT : cmd %s\n", CREAT_GRP);
#endif
      if (handle_creat_grp (cli_sd) != 0)
	goto error;
    }

    else {
      free (buf);
      goto done;
    }
    
    free (buf);
  }
  
 error:
  writeline (cli_sd, ERROR, strlen (ERROR));
#ifdef DEBUG
  printf ("*** ERROR\n");
#endif
  close (cli_sd);  
 done:
  return 0;
}



int
handle_grp_stat (int cli_sd)
{
  char out_buf[512];
  char * in_buf;

  char grp_id [PFS_ID_LEN];
  struct pfs_group * grp;
  struct pfs_sd * sd;

  int i;

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (grp_id, in_buf, PFS_ID_LEN);
  free (in_buf);

  writeline (cli_sd, OK, strlen (OK));

  pfs_mutex_lock (&pfsd->pfs->group_lock);  

  grp = pfsd->pfs->group;
  while (grp != NULL)
    {
      if (strncmp (in_buf, grp->grp_id, PFS_ID_LEN) == 0)
	goto found;
      grp = grp->next;
    }

  writeline (cli_sd, "0", 1);
  goto done;

 found:
  sprintf (out_buf, "%d", grp->sd_cnt);
  writeline (cli_sd, out_buf, strlen (out_buf));

  sd = grp->sd;
  while (sd != NULL)
    {
      writeline (cli_sd, sd->sd_id, PFS_ID_LEN);
      writeline (cli_sd, sd->sd_owner, strlen (sd->sd_owner));
      writeline (cli_sd, sd->sd_name, strlen (sd->sd_name));
      
      sprintf (out_buf, "%d", sd->sd_sv->len);
      writeline (cli_sd, out_buf, strlen (out_buf));
      
      for (i = 0; i < sd->sd_sv->len; i ++)
	{
	  writeline (cli_sd, sd->sd_sv->sd_id[i], PFS_ID_LEN);
	  sprintf (out_buf, "%llu", sd->sd_sv->value[i]);
	  writeline (cli_sd, out_buf, strlen (out_buf));	  
	}
      sd = sd->next;
    }

 done:
  pfs_mutex_unlock (&pfsd->pfs->group_lock);
  return 0;
 error:
  return -1;
}



int
handle_online (int cli_sd)
{
  char * in_buf;
  struct pfsd_sd * new_sd;
  struct pfsd_sd * sd;

  new_sd = (struct pfsd_sd *) malloc (sizeof (struct pfsd_sd));
  memset (new_sd, 0, sizeof (struct pfsd_sd));

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  new_sd->tun_conn = (uint8_t) atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  new_sd->tun_port = atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  sprintf (new_sd->sd_id, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (new_sd->sd_owner, in_buf, PFS_NAME_LEN);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (new_sd->sd_name, in_buf, PFS_NAME_LEN);
  free (in_buf);
  
  /* Received an sd connection, we add it to pfsd->sd */

  pfs_mutex_lock (&pfsd->sd_lock);
  
  sd = pfsd->sd;
  while (sd != NULL)
    {
      if (strncmp (new_sd->sd_id, sd->sd_id, PFS_ID_LEN) == 0 &&
	  sd->tun_conn == new_sd->tun_conn) {
	sd->tun_port = new_sd->tun_port;
	free (new_sd);
	goto done;
      }
      sd = sd->next;
    }
  
  new_sd->next = pfsd->sd;
  pfsd->sd = new_sd;
  pfsd->sd_cnt ++;

  pfsd->update = 1;

 done:
  pfs_mutex_unlock (&pfsd->sd_lock);

#ifdef DEBUG
  printf ("*** HANDLE_ONLINE : %s:%s : %.*s ",
	  new_sd->sd_owner,
	  new_sd->sd_name,
	  PFS_ID_LEN,
	  new_sd->sd_id);
  if (new_sd->tun_conn == LAN_CONN)
    printf ("(LAN:");
  if (new_sd->tun_conn == BTH_CONN)
    printf ("(BTH:");
  printf ("%d)\n", new_sd->tun_port);
#endif

  writeline (cli_sd, OK, strlen (OK));
  return 0;
 error:
  return -1;
}



int
handle_offline (int cli_sd)
{
  char * in_buf;
  struct pfsd_sd * prev;
  struct pfsd_sd * next;
  char sd_id [PFS_ID_LEN];
  uint8_t tun_conn;
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  tun_conn = (uint8_t) atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  sprintf (sd_id, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  /* Received an sd disconnection, we remove it */
  
  pfs_mutex_lock (&pfsd->sd_lock);
  
  prev = NULL;
  next = pfsd->sd;
  while (next != NULL)
    {
      if (strncmp (sd_id, next->sd_id, PFS_ID_LEN) == 0 &&
	  tun_conn == next->tun_conn) {
#ifdef DEBUG
	printf ("*** HANDLE_OFFLINE : %s:%s : %.*s ",
		next->sd_owner,
		next->sd_name,
		PFS_ID_LEN,
		next->sd_id);
	if (next->tun_conn == LAN_CONN)
	  printf ("(LAN:");
	if (next->tun_conn == BTH_CONN)
	  printf ("(BTH:");
	printf ("%d)\n", next->tun_port);
#endif
	
	if (prev == NULL) {
	  pfsd->sd = next->next;
	  free (next);
	  next = pfsd->sd;
	  pfsd->sd_cnt --;
	  continue;
	}
	else {
	  prev->next = next->next;
	  free (next);
	  next = prev->next;
	  pfsd->sd_cnt --;
	  continue;
	}
      }
      prev = next;
      next = next->next;
    }
  
  pfs_mutex_unlock (&pfsd->sd_lock);

  writeline (cli_sd, OK, strlen (OK));
  return 0;
 error:
  return -1;
}




int
handle_updt (int cli_sd)
{
  struct pfs_updt * updt = NULL;
  char * file_path = NULL;
  char * in_buf;
  int len;
  int b_done;
  int b_left;
  int b_tot;
  int fd;
  char buf[4096];
  int retval;
  struct stat st_buf;
  int progress;

  updt = net_read_updt (cli_sd);
  if (updt == NULL)
    goto error;
  
  /* Do we need data. */
  if (updt->ver->type == PFS_DIR) {
    file_path = pfs_mk_dir_path (pfsd->pfs,
				 updt->ver->dst_id);
    if (stat (file_path, &st_buf) != 0)
      pfs_create_dir_with_id (pfsd->pfs, updt->ver->dst_id);
    free (file_path);
    file_path = NULL;
    goto done;
  }

  if (updt->ver->type == PFS_SML ||
      updt->ver->type == PFS_FIL) 
    {
      file_path = pfs_mk_file_path (pfsd->pfs,
				    updt->ver->dst_id);
      if (stat (file_path, &st_buf) == 0) {
	free (file_path);
	goto done;
      }
    }
  else
    goto done;

  writeline (cli_sd, GET_DATA, strlen (GET_DATA));
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  len = atoi (in_buf);
  free (in_buf);
  
  if ((fd = open (file_path, 
		  O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    goto error;
  
  b_left = len;
  b_done = 0;
  b_tot = len;
  len = 1;
  progress = 0;
  while (b_left > 0 && len > 0)
    {
      len = read (cli_sd, buf, ((b_left > 4096) ? 4096 : b_left));
      writen (fd, buf, len);
      b_done += len;
      b_left -= len;
      if (((b_done * 100) / b_tot) >= (progress + 10) ||
	  ((b_done * 100) / b_tot) == 100) {
	progress = (b_done * 100) / b_tot;
#ifdef DEBUG
	printf ("*** RCVD %s : %d/100\n", updt->name, progress);
#endif
      }
      sprintf (buf, "%d", b_done);
      writeline (cli_sd, buf, strlen (buf));
    }

  if (b_left != 0) {
    close (fd);
    unlink (file_path);
    goto error;
  }

  close (fd);
  free (file_path);
  file_path = NULL;
  
 done:
  retval = pfs_set_entry (pfsd->pfs,
			  updt->grp_id,
			  updt->dir_id,
			  updt->name,
			  updt->reclaim,
			  updt->ver, 1);
  if (retval != 0 && retval != -2) {
    goto error;
  }

  pfs_free_updt (updt);
  writeline (cli_sd, OK, strlen (OK));

  return 0;

 error:
  if (updt != NULL)
    pfs_free_updt (updt);
  if (file_path != NULL)
    free (file_path);
  return -1;
}




/*
 * NOTE : For now we do not revoke log entries
 * when propagated so adding an sd is really
 * easy : we just have to create the group.
 */

int
handle_add_sd (int cli_sd)
{
  char * in_buf;
  char sd_owner[PFS_NAME_LEN];
  char sd_name[PFS_NAME_LEN];
  char grp_id [PFS_ID_LEN];
  char grp_name [PFS_NAME_LEN];
  char sd_id [PFS_ID_LEN];

  int tun_port = -1;
  uint8_t tun_conn = 100;
  struct pfsd_sd * sd;

  int tun_sd = -1;
  struct hostent *he;
  struct sockaddr_in tun_addr;

  memset (grp_name, 0, PFS_NAME_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (grp_name, in_buf, PFS_NAME_LEN);
  free (in_buf);

  memset (sd_owner, 0, PFS_NAME_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (sd_owner, in_buf, PFS_NAME_LEN);
  free (in_buf);

  memset (sd_name, 0, PFS_NAME_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (sd_name, in_buf, PFS_NAME_LEN);
  free (in_buf);
  

  /* Go fetch the grp_id. */

  if (pfs_get_grp_id (pfsd->pfs, grp_name, grp_id) != 0)
    goto error;


  /* Go fetch the id. */

  pfs_mutex_lock (&pfsd->sd_lock);
  sd = pfsd->sd;
  while (sd != NULL)
    {
      if (strncmp (sd_owner, sd->sd_owner, PFS_NAME_LEN) == 0 &&
	  strncmp (sd_name, sd->sd_name, PFS_NAME_LEN) == 0 &&
	  sd->tun_conn < tun_conn) {
	tun_port = sd->tun_port;
	tun_conn = sd->tun_conn;
	strncpy (sd_id, sd->sd_id, PFS_ID_LEN);
      }
      sd = sd->next;
    }
  
  if (tun_port == -1) {
    pfs_mutex_unlock (&pfsd->sd_lock);
    goto error;
  }
  pfs_mutex_unlock (&pfsd->sd_lock);
  
  
  /* 
   * We try to connect to the sd. 
   */
  
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
  if (strcmp (in_buf, OK) != 0) goto error;
  free (in_buf);
  
  writeline (tun_sd, ADD_GRP, strlen (ADD_GRP));
  writeline (tun_sd, grp_name, strlen (grp_name));
  writeline (tun_sd, grp_id, PFS_ID_LEN);
  writeline (tun_sd, pfsd->pfs->sd_id, PFS_ID_LEN);
  
  if (pfs_group_add_sd (pfsd->pfs, grp_id, sd_id,
			sd_owner, sd_name) != 0)
    goto error;

  /* 
   * We do not propagate data here.
   * For now we keep all log updates.
   */

  /*
   * We update the remote grp_sate.
   */

  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;
  if (strcmp (in_buf, GRP_STAT) != 0) {
    free (in_buf);
    goto error;
  }
  free (in_buf);

  handle_grp_stat (tun_sd);

  in_buf = readline (tun_sd);
  if (in_buf == NULL) goto error;
  if (strcmp (in_buf, OK) != 0) {
    free (in_buf);
    goto error;
  }
  free (in_buf);

  writeline (tun_sd, CLOSE, strlen (CLOSE));

  close (tun_sd);

  /* We add the sd to our local state. */

  pfsd->update = 1;

  writeline (cli_sd, OK, strlen (OK));
  return 0;

 error:  
  if (tun_sd != -1)
    close (tun_sd);
  return -1;
}




int
handle_add_grp (int cli_sd)
{
  char grp_name [PFS_NAME_LEN];
  char grp_id [PFS_NAME_LEN];
  char sd_id [PFS_ID_LEN];
  char * in_buf;

  memset (grp_name, 0, PFS_NAME_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (grp_name, in_buf, PFS_NAME_LEN);
  free (in_buf);

  memset (grp_id, 0, PFS_ID_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (grp_id, in_buf, PFS_ID_LEN);
  free (in_buf);

  memset (sd_id, 0, PFS_ID_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (sd_id, in_buf, PFS_ID_LEN);
  free (in_buf);

  if (pfs_create_dir_with_id (pfsd->pfs, grp_id) != 0)
    goto error;
  if (pfs_group_add (pfsd->pfs, grp_name, grp_id) != 0)
    goto error;

#ifdef DEBUG  
  printf ("*** PFS_GRP_CREATE %.*s : %s\n", PFS_ID_LEN, grp_id, grp_name);
#endif

  if (update_status (cli_sd, grp_id, sd_id) != 0)
    goto error;
		     
  writeline (cli_sd, OK, strlen (OK));
  return 0;
  
 error:
  return -1;
}



int
handle_creat_grp (int cli_sd)
{
  char grp_name [PFS_NAME_LEN];
  char * in_buf;

  memset (grp_name, 0, PFS_NAME_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (grp_name, in_buf, PFS_NAME_LEN);
  free (in_buf);
  
  if (pfs_group_create (pfsd->pfs, grp_name) != 0)
    goto error;
  
  writeline (cli_sd, OK, strlen (OK));
  return 0;

 error:
  return -1;
}


int
handle_list_sd (int cli_sd)
{
  char out_buf [512];
  struct pfsd_sd * sd;

  writeline (cli_sd, OK, strlen (OK));

  pfs_mutex_lock (&pfsd->sd_lock);
  
  sprintf (out_buf, "%d", pfsd->sd_cnt);
  writeline (cli_sd, out_buf, strlen (out_buf));
  
  sd = pfsd->sd;
  while (sd != NULL)
    {
      if (sd->tun_conn == LAN_CONN)
	writeline (cli_sd, "LAN", strlen ("LAN"));
      if (sd->tun_conn == BTH_CONN)
	writeline (cli_sd, "BTH", strlen ("BTH"));
      writeline (cli_sd, sd->sd_owner, strlen (sd->sd_owner));
      writeline (cli_sd, sd->sd_name, strlen (sd->sd_name));
      writeline (cli_sd, sd->sd_id, PFS_ID_LEN);
      sd = sd->next;
    }
  
  pfs_mutex_unlock (&pfsd->sd_lock);
  return 0;
}




/**************************************************/

struct pfs_updt *
net_read_updt (int cli_sd)
{
  struct pfs_updt * updt;
  char * in_buf;
  int i;

  updt = (struct pfs_updt *) malloc (sizeof (struct pfs_updt));
  memset (updt, 0, sizeof (struct pfs_updt));

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->grp_id, in_buf, PFS_ID_LEN);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->dir_id, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->name, in_buf, PFS_NAME_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  updt->reclaim = (uint8_t) atoi (in_buf);
  free (in_buf); 
 
  updt->ver = (struct pfs_ver *) malloc (sizeof (struct pfs_ver));
  memset (updt->ver, 0, sizeof (struct pfs_ver));

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  updt->ver->type = (uint8_t) atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  updt->ver->st_mode = (int) atoi (in_buf);
  free (in_buf);

  memset (updt->ver->dst_id, 0, PFS_ID_LEN);
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->ver->dst_id, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->ver->last_updt, in_buf, PFS_ID_LEN);
  free (in_buf);

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  strncpy (updt->ver->sd_orig, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  updt->ver->cs = atol (in_buf);
  free (in_buf);

  updt->ver->mv = (struct pfs_vv *) malloc (sizeof (struct pfs_vv));
  memset (updt->ver->mv, 0, sizeof (struct pfs_vv));

  in_buf = readline (cli_sd);
  if (in_buf == NULL) goto error;
  updt->ver->mv->len = atoi (in_buf);
  free (in_buf);
  
  updt->ver->mv->sd_id = (char **) malloc (updt->ver->mv->len * sizeof (char *));
  updt->ver->mv->value = (uint64_t *) malloc (updt->ver->mv->len * sizeof (uint64_t));
  for (i = 0; i < updt->ver->mv->len; i ++)
    {
      updt->ver->mv->sd_id[i] = (char *) malloc (PFS_ID_LEN);
    }

  for (i = 0; i < updt->ver->mv->len; i ++)
    {
      in_buf = readline (cli_sd);
      if (in_buf == NULL) goto error;
      strncpy (updt->ver->mv->sd_id[i], in_buf, PFS_ID_LEN);
      free (in_buf);
      
      in_buf = readline (cli_sd);
      if (in_buf == NULL) goto error;
      updt->ver->mv->value[i] = atol (in_buf);
      free (in_buf);
    }  

  return updt;

 error:
  if (updt != NULL) {
    if (updt->ver != NULL) {
      if (updt->ver->mv != NULL)
	pfs_free_ver (updt->ver);
      else
	free (updt->ver);
    }
    free (updt);
  }
  return NULL;
}

