#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "io.h"


size_t
readn (int fd, void *vptr, size_t n)
{
  ssize_t nleft;
  ssize_t nread;
  char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = read (fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;
      else
        return -1;
    }
    else if (nread == 0)
      break;
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);
}


size_t
writen (int fd, const void *vptr, size_t n)
{
  ssize_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write (fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;
      else
        return -1;
    }
    if (nwritten < 0)
      return -1;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}


char *
readline (int fd)
{
  char * line;
  char * ptr;
  int i = 0;
  int nread = 0;
  int len = 0;
  int buflen = 0;

  if ((line = (char *) malloc (LINE_BUF_SIZE * sizeof (char))) == NULL)
    return NULL;
  ptr = line;

  /* we accept \r\n and \n as endline. */

  while (len < (MAX_LINE_BUF_SIZE-1) && (nread = readn (fd, ptr, 1)) == 1
         && *ptr != '\n') {

    if (*ptr != '\r') {
      len += nread;
      ptr += nread;
      buflen += nread;

      if (buflen == LINE_BUF_SIZE) {
        i ++;
        if ((line = realloc (line, (i+1) * LINE_BUF_SIZE * sizeof (char))) == NULL) {
          free (line);
          return NULL;
        }
        ptr = line + i * LINE_BUF_SIZE;
        buflen = 0;
      }
    }
  }

  if (nread == -1 || len >= MAX_LINE_BUF_SIZE) {
    free (line);
    return NULL;
  }
  line [len] = 0;
  return line;
}

size_t
writeline (int fd, char *ptr, size_t n)
{
  size_t r = 0;
  r += writen (fd, ptr, n);
  r += writen (fd, "\n", 1);
  return r;
}
