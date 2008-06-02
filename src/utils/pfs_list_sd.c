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

#include "../pfsd/global.h"
#include "../libpfs/pfs.h"
#include "../libpfs/lib/io.h"

#define PFSD_PORT 9999

int 
main (int argc, char ** argv)
{
  int pfsd_sd = -1;
  struct hostent *he;
  struct sockaddr_in pfsd_addr;
  
  char * in_buf;
  int sd_cnt;
  int i;

  if (argc != 1) {
    printf ("usage : pfs_list_sd\n");
    exit (-1);
  }

  /* 
   * We try to connect to the local pfsd server. 
   */
  
  if ((pfsd_sd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    goto error;
  if ((he = gethostbyname ("localhost")) == NULL)
    goto error;
  
  bzero (&pfsd_addr, sizeof (pfsd_addr));
  pfsd_addr.sin_family = AF_INET;
  pfsd_addr.sin_port = htons (PFSD_PORT);
  pfsd_addr.sin_addr = *((struct in_addr *) he->h_addr);
  
  if (connect(pfsd_sd, (struct sockaddr *)&pfsd_addr,
	      sizeof (pfsd_addr)) < 0)
    goto error;
  
  writeline (pfsd_sd, LIST_SD, strlen (CREAT_GRP));
  

  in_buf = readline (pfsd_sd);
  if (in_buf == NULL) goto error;
  if (strcmp (in_buf, OK) != 0) {
    free (in_buf);
    goto error;
  }
  free (in_buf);

  in_buf = readline (pfsd_sd);
  if (in_buf == NULL) goto error;
  sd_cnt = atoi (in_buf);
  free (in_buf);

  for (i = 0; i < sd_cnt; i ++) {
    in_buf = readline (pfsd_sd);
    if (in_buf == NULL) goto error;
    //printf ("%s : ", in_buf);
    free (in_buf);

    in_buf = readline (pfsd_sd);
    if (in_buf == NULL) goto error;
    printf ("%s.", in_buf);
    free (in_buf);

    in_buf = readline (pfsd_sd);
    if (in_buf == NULL) goto error;
    printf ("%s ", in_buf);
    free (in_buf);

    in_buf = readline (pfsd_sd);
    if (in_buf == NULL) goto error;
    printf ("(%s)\n", in_buf);
    free (in_buf);
  }
  
  writeline (pfsd_sd, CLOSE, strlen (CLOSE));

  close (pfsd_sd);

  return 0;

 error:
  printf ("could not list sd.\n");
  return -1;
}
