#include <stdlib.h>
#include <unistd.h>

#include "nopfs.h"

int main (int argc, char ** argv)
{
  fuse_main (argc, argv, &nopfs_operations, NULL);
  exit (0);
}

