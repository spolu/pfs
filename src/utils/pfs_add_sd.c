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


int 
main (int argc, char ** argv)
{
  char grp_name [PFS_NAME_LEN];
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];

  int pfsd_sd = -1;
  struct hostent *he;
  struct sockaddr_in pfsd_addr;
  
  unsigned short pfsd_port;
  char * in_buf;

  if (argc != 5) {
    printf ("usage : pfs_add_sd port grp_name sd_owner sd_name\n");
    exit (-1);
  }

  if (strlen (argv[2]) > PFS_NAME_LEN - 1 &&
      strlen (argv[3]) > PFS_NAME_LEN - 1 &&
      strlen (argv[4]) > PFS_NAME_LEN - 1) {
    printf ("grp_name, sd_owner, sd_name must be %d character max\n", PFS_NAME_LEN - 1);
  }

  strncpy (grp_name, argv[2], PFS_NAME_LEN);
  strncpy (sd_owner, argv[3], PFS_NAME_LEN);
  strncpy (sd_name, argv[4], PFS_NAME_LEN);
  pfsd_port = atoi (argv[1]);

  /* 
   * We try to connect to the local pfsd server. 
   */
  
  if ((pfsd_sd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    goto error;
  if ((he = gethostbyname ("localhost")) == NULL)
    goto error;
  
  bzero (&pfsd_addr, sizeof (pfsd_addr));
  pfsd_addr.sin_family = AF_INET;
  pfsd_addr.sin_port = htons (pfsd_port);
  pfsd_addr.sin_addr = *((struct in_addr *) he->h_addr);
  
  if (connect(pfsd_sd, (struct sockaddr *)&pfsd_addr,
	      sizeof (pfsd_addr)) < 0)
    goto error;
  
  writeline (pfsd_sd, ADD_SD, strlen (CREAT_GRP));
  writeline (pfsd_sd, grp_name, strlen (grp_name));
  writeline (pfsd_sd, sd_owner, strlen (sd_owner));
  writeline (pfsd_sd, sd_name, strlen (sd_name));
  
  in_buf = readline (pfsd_sd);
  if (in_buf == NULL) goto error;
  printf ("received : %s\n", in_buf);
  if (strcmp (in_buf, OK) != 0) {
    free (in_buf);
    goto error;
  }
  free (in_buf);
  
  writeline (pfsd_sd, CLOSE, strlen (CLOSE));

  close (pfsd_sd);

  printf ("sd %s.%s added to group %s.\n", sd_owner, sd_name,  grp_name);

  return 0;

 error:
  printf ("could not add sd.\n");
  return -1;
}
