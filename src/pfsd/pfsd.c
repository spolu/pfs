#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "pfsd.h"
#include "fuse/pfs_fuse.h"
#include "../libpfs/updt.h"

int
main (int argc, char ** argv)
{
  char root_path [PFS_NAME_LEN];
  
  pthread_t threads [1];

  if (argc <= 2) {
    printf ("usage : ./pfs root_path [FUSE ARGS]\n");
    exit (1);
  }

  strncpy (root_path, argv[1], PFS_NAME_LEN);

  pfs = pfs_init (root_path);
  
  strncpy (argv[1], argv[0], strlen (argv[1]));
  argv ++;
  argc --;

  pfs_set_updt_cb (pfs, updt_cb);

  if (pthread_create (&threads[0], NULL, start_write_back, (void *)0) != 0) {
    exit (1);
  }

  fuse_main (argc, argv, &pfs_operations, NULL);
  
  exit (0);
}


void *
start_write_back (void * tid)
{
  while (1) {
    sleep (30);
    pfs_sync_cache (pfs);
  }
}

int
updt_cb (struct pfs_instance * pfs,
	 struct pfs_updt * updt)
{
  //pfs_print_updt (updt);
  return 0;
}
