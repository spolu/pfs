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
  srv_addr.sin_port = htons (PFSD_PORT);
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

  printf ("Spawning pfsd_srv thread\n");

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
      if (handle_status (cli_sd) != 0)
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
      if (handle_updt (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (ADD_SD, buf) == 0) {
      if (handle_add_sd (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (ADD_GRP, buf) == 0) {
      if (handle_add_grp (cli_sd) != 0)
	goto error;
    }

    else if (strcmp (LIST_SD, buf) == 0) {
      if (handle_list_sd (cli_sd) != 0)
	goto error;
    }

    else {
      free (buf);
      goto done;
    }
    
    free (buf);
  }
  
 error:
 done:
  printf ("Closing connection to client\n");
  writeline (cli_sd, CLOSE, strlen (CLOSE));
  close (cli_sd);  
  return 0;
}



int
handle_status (int cli_sd)
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
	  sprintf (out_buf, "%ld", sd->sd_sv->value[i]);
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
  
  writeline (cli_sd, OK, strlen (OK));

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

 done:
  pfs_mutex_unlock (&pfsd->sd_lock);
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
  
  writeline (cli_sd, OK, strlen (OK));

  /* Received an sd disconnection, we remove it */
  
  pfs_mutex_lock (&pfsd->sd_lock);
  
  prev = NULL;
  next = pfsd->sd;
  while (next != NULL)
    {
      if (strncmp (sd_id, next->sd_id, PFS_ID_LEN) == 0 &&
	  tun_conn == next->tun_conn) {
	if (prev == NULL) {
	  pfsd->sd = next->next;
	  free (next);
	  next = pfsd->sd;
	  continue;
	}
	else {
	  prev->next = next->next;
	  free (next);
	  next = prev->next;
	  continue;
	}
      }
      prev = next;
      next = next->next;
    }
  
  pfs_mutex_unlock (&pfsd->sd_lock);
  return 0;
 error:
  return -1;
}




int
handle_updt (int cli_sd)
{
  struct pfs_updt * updt;
  char * file_path = NULL;
  struct stat st_buf;
  char * in_buf;
  int len;
  int b_done;
  int b_left;
  int fd;
  char buf[4096];

  updt = net_read_updt (cli_sd);
  if (updt == NULL)
    goto error;
  
  
  /* Do we need data. */
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
		  O_CREAT | O_WRONLY | O_APPEND, 
		  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
    goto error;
  
  b_left = len;
  b_done = 0;
  while (b_left > 0)
    {
      len = read (cli_sd, buf, ((b_left > 4096) ? 4069 : b_left));
      if (len == -1) {
	close (fd);
	unlink (file_path);
	goto error;
      }
      writen (fd, buf, len);
      b_done += len;
      b_left -= len;
    }

  close (fd);
  free (file_path);
  
 done:
  pfs_set_entry (pfsd->pfs,
		 updt->grp_id,
		 updt->dir_id,
		 updt->name,
		 updt->reclaim,
		 updt->ver);

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




int
handle_add_sd (int cli_sd)
{
  /*
   * TODO : sd addition todo in blocking fashion here.
   */
  return -1;
}




int
handle_add_grp (int cli_sd)
{
  /*
   * TODO : other side of add_sd
   */
  return -1;
}




int
handle_list_sd (int cli_sd)
{
  char out_buf [512];
  struct pfsd_sd * sd;

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

