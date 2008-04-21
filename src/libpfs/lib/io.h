#ifndef _PFS_IO_H
#define _PFS_IO_H

#define LINE_BUF_SIZE 512
#define MAX_LINE_BUF_SIZE 8192

#include <sys/types.h>
#include <unistd.h>

size_t readn (int fd, void *vptr, size_t n);
size_t writen (int fd, const void *vptr, size_t n);
size_t preadn (int fd, void *vptr, size_t n, off_t offset);
size_t pwriten (int fd, const void *vptr, size_t n, off_t offset);
char * readline (int fd);
size_t writeline (int fd, char *ptr, size_t n);

#endif
