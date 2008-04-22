#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "../libpfs/pfs.h"

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

  printf ("Initiating %s.%s in dir %s\n", root_path, sd_owner, sd_name);
  
  pfs_bootstrap (root_path, sd_owner, sd_name);
  
  exit (0);
}

