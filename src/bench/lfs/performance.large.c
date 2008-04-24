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

#define SIZE	8192

static char buf[SIZE];
static char name[32] = "test_file";
static char *prog_name;

extern int errno;

int do_fsync;

#define TRULY_RANDOM

int f(int i, int n)
{
  return ((i * 11) % n);
}

int write_test(int n, int size, int sequential)
{
  int i;
  int r;
  int fd;
  long pos = 0;
  struct stat statb;
  char buf1[128];
  char buf2[128];
  struct timeval tv;
  time_t x;
  struct tm *tmp;
  unsigned s, fin;

  i = 0;
  s = time(0);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("START WRITE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);
    
  if((fd = open(name, O_RDWR)) < 0) {
    printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
    exit(1);
  }

  for (i = 0; i < n; i ++) {
    if (!sequential) {

#ifdef TRULY_RANDOM
      pos = (random() % n) * size;
#else
      pos = f(i, n) * size;
#endif
      
      if ((r = lseek(fd, pos, 0)) < 0) {
	printf("%s: lseek failed %d %d\n", prog_name, r, errno);
      }
    }

    if ((r = write(fd, buf, size)) < 0) {
      printf("%s: write failed %d %d (%ld)\n", prog_name, r, errno,
	     pos);
      exit(1);
    }
  }
    
  if (do_fsync)
    fsync(fd);

  fstat(fd, &statb);
  if (fchown(fd, statb.st_uid, -1) < 0) {
    perror("fchown");
  }

  if ((r = close(fd)) < 0) {
    printf("%s: close failed %d %d\n", prog_name, r, errno);
  }

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END WRITE : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  fin = time(0);
  printf("%s: write took %d sec\n", prog_name, fin - s);

  return 0;
}


int g(int i, int n)
{
  if (i % 2 == 0) return(n / 2 + i / 2);
  else return(i / 2);
}


int read_test(int n, int size, int sequential)
{
  int i;
  int r;
  int fd;
  long pos;
  char buf1[128];
  char buf2[128];
  struct timeval tv;
  time_t x;
  struct tm *tmp;
  unsigned s, fin;
  i = 0;

  s = time(0);

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("START READ : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);
    
  if((fd = open(name, O_RDONLY)) < 0) {
    printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
    exit(1);
  }

  for (i = 0; i < n; i ++) {
    
    if (!sequential) {
      
#ifdef TRULY_RANDOM
      pos = (random() % n) * size;
#else
      pos = g(i, n) * size;
#endif

      if ((r = lseek(fd, pos, 0)) < 0) {
	printf("%s: lseek failed %d %d\n", prog_name, r, errno);
      }
    }

    if ((r = read(fd, buf, size)) < 0) {
      printf("%s: read failed %d %d\n", prog_name, r, errno);
      exit(1);
    }
  }
    
  if ((r = close(fd)) < 0) {
    printf("%s: close failed %d %d\n", prog_name, r, errno);
  }
    

  gettimeofday (&tv, NULL);
  x = tv.tv_sec;
  tmp = localtime (&x);
  strftime (buf1, sizeof (buf1), "%a %b %e %H:%M:%S", tmp);
  strftime (buf2, sizeof (buf2), "%Z %Y", tmp);
  printf ("END READ : %s.%06d %s\n", buf1, (int) tv.tv_usec, buf2);

  fin = time(0);
  printf("%s: read took %d sec\n",
	 prog_name,
	 fin - s);

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
  int n, fd;
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

  printf("\n%s %d %d\n", prog_name, n, size);

  srandom(getpid());

  if((fd = creat(name, S_IRUSR | S_IWUSR)) < 0) {
    printf("%s: create %d failed %d\n", prog_name, fd, errno);
    exit(1);
  }
  close (fd);

  flush_cache ();
  write_test(n, size, 1);
  read_test(n, size, 1);
  write_test(n , size, 0);
  read_test(n, size, 0);
  read_test(n , size, 1);

  unlink(name);
}



