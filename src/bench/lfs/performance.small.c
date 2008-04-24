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
#include <errno.h>

static char buf[40960];
static char name[32];
static char *prog_name;
extern int errno;

int do_fsync;

#define NDIR 100

static char dir[32];

void creat_dir()
{
  int i;

  umask(0);

  for (i = 0; i < NDIR; i++) {
    sprintf(dir, "d%d", i);
    mkdir(dir, 0777);
  }
}

void rm_dir()
{
  int i;

  umask(0);

  for (i = 0; i < NDIR; i++) {
    sprintf(dir, "d%d", i);
    rmdir(dir);
  }
}


int creat_test(int n, int size)
{
  int i;
  int r;
  int fd = 0;
  int j;
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

    sprintf(name, "d%d/g%d", j, i);

    if((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
      printf("%s: create %d failed %d %d\n", prog_name, i, fd, errno);
      exit(1);
    }

    if ((r = write(fd, buf, size)) < 0) {
      printf("%s: write failed %d %d\n", prog_name, r, errno);
      exit(1);
    }

    if ((r = close(fd)) < 0) {
      printf("%s: close failed %d %d\n", prog_name, r, errno);
    }

    if ((i+1) % 100 == 0) j++;  
  }

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


int write_test(int n, int size)
{
  int i = 0;
  int j = 0;
  int r;
  int s;
  int fd = 0;
  long pos = 0;

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
  printf ("START WRITE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);
    
  for (i = 0, j = 0; i < n; i ++) {
    
    sprintf(name, "d%d/g%d", j, i);
    
    if((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
      printf("%s: create %d failed %d %d\n", prog_name, i, fd, errno);
      exit(1);
    }
    
    if ((r = write(fd, buf, size)) < 0) {
      printf("%s: write failed %d %d (%ld)\n", prog_name, r, errno,
	     pos);
      exit(1);
    }
    
    if (do_fsync)
      fsync (fd);
    
    if ((r = close(fd)) < 0) {
      printf("%s: mnx_close failed %d %d\n", prog_name, r, errno);
    }

    if ((i+1) % 100 == 0) j++;  
  }

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END WRITE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  return 0;
}


int read_test(int n, int size)
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

    sprintf(name, "d%d/g%d", j, i);

    if((fd = open(name, O_RDONLY)) < 0) {
      printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
      exit(1);
    }

    if ((r = read(fd, buf, size)) < 0) {
      printf("%s: read failed %d %d\n", prog_name, r, errno);
      exit(1);
    }

    if ((r = close(fd)) < 0) {
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

int delete_test(int n)
{	
  int i;
  int r;
  int fd = 0;
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

    sprintf(name, "d%d/g%d", j, i);

    if ((r = unlink(name)) < 0) {
      printf("%s: unlink failed %d\n", prog_name, r);
      exit(1);
    }

    if ((i+1) % 100 == 0) j++;
  }

  fsync(fd);

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


int flush_cache()
{
  int i, r, fd;
  i = 0;

  if((fd = open("t", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
    printf("%s: create %d failed %d %d\n", prog_name, i, fd, errno);
    exit(1);
  }

  for (i = 0; i < 20000; i ++) {
    if ((r = write(fd, buf, 4096)) < 0) {
      printf("%s: write failed %d %d\n", prog_name, r, errno);
      exit(1);
    }
  }
    
  fsync(fd);

  if ((r = close(fd)) < 0) {
    printf("%s: mnx_close failed %d %d\n", prog_name, r, errno);
  }

  unlink("t");

  return 0;
}


int main(int argc, char *argv[])
{
  int n;
  int size;

  prog_name = argv[0];

  if (argc != 4) {
    printf("%s: %s num size fsync\n", prog_name, prog_name);
    exit(1);
  }

  n = atoi(argv[1]);
  size = atoi(argv[2]);
  do_fsync = atoi (argv[3]);

  if (do_fsync != 0 && do_fsync != 1) {
    printf("%s: fsync 0 or 1!\n", prog_name);    
    exit (1);
  }

  if (n > 100 * NDIR) {
    printf ("%s: num must not be biger than 100 times %d\n", prog_name, NDIR); 
    exit (1);
  }

  printf("\n%s %d %d\n", prog_name, n, size);

  creat_dir();
  flush_cache ();
  creat_test(n, size);
  read_test(n, size);
  write_test(n, size);
  delete_test(n);
  rm_dir();

  return 0;
}
