#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "../../libpfs/pfs.h"

static char buf[40960];
static char name[64];
static char *prog_name;

extern int errno;


#define NDIR 100


static char dir[64];

void creat_dir(struct pfs_instance * pfs)
{
  int i;

  for (i = 0; i < NDIR; i++) {
    sprintf(dir, "/me/d%d", i);
    pfs_mkdir(pfs, dir);
  }
}



int creat_test(struct pfs_instance * pfs, int n, int size)
{
  int i;
  int r;
  int fd;
  int j;
  //struct stat statb;
  char buf1[128];
  char buf2[128];
  struct timeval tv;
  time_t x;
  struct tm *tmp;  
  unsigned s, f;

  s = time(0);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("START CREAT : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);


  for (i = 0, j = 0; i < n; i ++) {

    sprintf(name, "/me/d%d/g%d", j, i);
    //sprintf(name, "/me/g%d", i);

    if((fd = pfs_open(pfs, name, O_RDWR | O_CREAT | O_TRUNC)) < 0) {
      printf("%s: create %d failed %d %d\n", prog_name, i, fd, errno);
      exit(1);
    }

    if ((r = pfs_pwrite(pfs, fd, buf, size, 0)) < 0) {
      printf("%s: write failed %d %d\n", prog_name, r, errno);
      exit(1);
    }

    if ((r = pfs_close(pfs, fd)) < 0) {
      printf("%s: close failed %d %d\n", prog_name, r, errno);
    }

    if ((i+1) % 100 == 0) j++;

  }

  //fsync(fd);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END CREAT : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  f = time(0);
  printf("%s: creat took %d sec\n",  prog_name,  f - s);

  return 0;
}


int read_test(struct pfs_instance * pfs, int n, int size)
{
  int i;
  int r;
  int fd;
  int j;
  unsigned s, f;

  char buf1[128];
  char buf2[128];
  struct timeval tv;
  time_t x;
  struct tm *tmp;  

  s = time(0);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("START READ : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  for (i = 0, j = 0; i < n; i ++) {

    sprintf(name, "/me/d%d/g%d", j, i);
    //sprintf(name, "/me/g%d", i);

    if((fd = pfs_open(pfs, name, O_RDONLY)) < 0) {
      printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
      exit(1);
    }

    if ((r = pfs_pread(pfs, fd, buf, size, 0)) < 0) {
      printf("%s: read failed %d %d\n", prog_name, r, errno);
      exit(1);
    }

    if ((r = pfs_close(pfs, fd)) < 0) {
      printf("%s: close failed %d %d\n", prog_name, r, errno);
    }

    if ((i+1) % 100 == 0) j++;
  }

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END READ : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  f = time(0);
  printf("%s: read took %d sec\n",
	 prog_name,
	 f - s);
    
  return 0;
}

int delete_test(struct pfs_instance * pfs, int n)
{	
  int i;
  int r;
  int j;
 
  unsigned s, f;

  char buf1[128];
  char buf2[128];
  struct timeval tv;
  time_t x;
  struct tm *tmp;  

  s = time(0);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("START DELETE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  for (i = 0, j = 0; i < n; i ++) {

    sprintf(name, "/me/d%d/g%d", j, i);
    //sprintf(name, "/me/g%d", i);

    if ((r = pfs_unlink(pfs, name)) < 0) {
      printf("%s: unlink failed %d\n", prog_name, r);
      exit(1);
    }

    if ((i+1) % 100 == 0) j++;
  }

  //fsync(fd);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END DELETE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  f = time(0);
  printf("%s: unlink took %d sec\n",
	 prog_name,
	 f - s);

  return 0;
}


int main(int argc, char *argv[])
{
  int n;
  int size;
  struct pfs_instance * pfs;

  prog_name = argv[0];

  //if (argc != 3) {
  // printf("%s: %s num size\n", prog_name, prog_name);
  // exit(1);
  //}

  pfs = pfs_init ("/home/spolu/.pfs/");

  n = 10000;
  size = 4096;

  printf("%s %d %d\n", prog_name, n, size);

  creat_dir(pfs);
  creat_test(pfs, n, size);
  read_test(pfs, n, size);
  delete_test(pfs, n);
}
