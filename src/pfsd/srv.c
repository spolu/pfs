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

    else {
      free (buf);
      goto done;
    }
    
    free (buf);
  }
  
 error:
 done:
  printf ("Closing connection to client\n");
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
  new_sd->tun_conn = (uint8_t) atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  new_sd->tun_port = atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
  sprintf (new_sd->sd_id, in_buf, PFS_ID_LEN);
  free (in_buf);
  
  in_buf = readline (cli_sd);
  strncpy (new_sd->sd_owner, in_buf, PFS_NAME_LEN);
  free (in_buf);

  in_buf = readline (cli_sd);
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
    }
  
  new_sd->next = pfsd->sd;
  pfsd->sd = new_sd;
  pfsd->sd_cnt ++;

 done:
  pfs_mutex_unlock (&pfsd->sd_lock);
  return 0;
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
  tun_conn = (uint8_t) atoi (in_buf);
  free (in_buf);

  in_buf = readline (cli_sd);
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
    }
  
  pfs_mutex_unlock (&pfsd->sd_lock);
  return 0;
}


int
handle_updt (int cli_sd)
{
  
  return 0;
}

int
handle_add_sd (int cli_sd)
{
  
  return 0;
}
