#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <signal.h>
#include <fcntl.h>

#include "../libpfs/pfs.h"
#include "../pfsd/pfsd.h"

int
pfsd_bootstrap (const char * root_path)
{
  char * log_path;
  int fd;
  uint32_t zero = 0;

  log_path = (char *) malloc (strlen (root_path) + 
			      strlen (PFSD_LOG_PATH) + 1);
  sprintf (log_path, "%s%s", root_path, PFSD_LOG_PATH);

  fd = open (log_path, O_WRONLY|O_TRUNC|O_APPEND|O_CREAT);
  fchmod (fd, S_IRUSR | S_IWUSR);
  write (fd, &zero, sizeof (uint32_t));
  close (fd);

  return 0;
}


int main (int argc, char ** argv)
{
  char root_path [PFS_NAME_LEN];
  char sd_owner [PFS_NAME_LEN];
  char sd_name [PFS_NAME_LEN];
  
  if (argc != 4) {
    printf ("usage : ./pfs_init root_path sd_owner sd_name\n");
    exit (1);
  }

  strncpy (root_path, argv[1], PFS_NAME_LEN);
  strncpy (sd_owner, argv[2], PFS_NAME_LEN);
  strncpy (sd_name, argv[3], PFS_NAME_LEN);

  printf ("Initiating %s.%s in dir %s\n", sd_owner, sd_name, root_path);
  
  pfs_bootstrap (root_path, sd_owner, sd_name);
  pfsd_bootstrap (root_path);
  
  exit (0);
}

