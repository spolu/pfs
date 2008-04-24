#include "stub.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>


int nopfs_fuse_getattr (const char *path, struct stat *stbuf)
{
  int retval;
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  retval = stat (new_path, stbuf);
  
  if (retval < 0)
    return -errno;
  return 0;
}


int nopfs_fuse_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
  DIR * dirp;
  struct dirent * dp;

  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);


  dirp = opendir(new_path);
  while ((dp = readdir(dirp)) != NULL) {
    filler (buf, dp->d_name, NULL, 0);
  }
  (void)closedir(dirp);
  return 0;
}


int nopfs_fuse_open (const char *path, struct fuse_file_info *fi)
{
  int fd;
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  if ((fd = open (new_path, fi->flags)) < 0) {
    return fd;
  }
  fi->fh = fd;
  return 0;
}

int nopfs_fuse_read (const char *path, char *buf, size_t size, off_t offset,
		     struct fuse_file_info *fi)
{
  ssize_t lenr;
  
  if (lseek ((int) fi->fh, offset, SEEK_SET) != offset ||
      (lenr = read ((int) fi->fh, buf, size)) < 0)
    return -errno;
  
  return lenr;
}

int nopfs_fuse_write (const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
  ssize_t lenw;
  
  if (lseek ((int) fi->fh, offset, SEEK_SET) != offset ||
      (lenw = write ((int) fi->fh, buf, size)) < 0)
    return -errno;

  return lenw;
}

int nopfs_fuse_fsync (const char *path, int param, struct fuse_file_info *fi)
{
  return fsync ((int) fi->fh);
}

int nopfs_fuse_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
  return ftruncate ((int) fi->fh, offset);
}

int nopfs_fuse_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
  int fd;
  
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  fi->flags |= O_CREAT;
  if ((fd = open (new_path, fi->flags)) < 0) {
    return -errno;
  }
  chmod (new_path, S_IRWXU);
  fi->fh = fd;
  return 0;
}

int nopfs_fuse_release (const char *path, struct fuse_file_info *fi)
{

  return close ((int) fi->fh);
}

int nopfs_fuse_truncate (const char *path, off_t offset)
{
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  return truncate (new_path, offset);
}

int nopfs_fuse_mkdir (const char *path, mode_t mode)
{
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  return mkdir (new_path, mode);
}

int nopfs_fuse_unlink (const char *path)
{
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  int retval = unlink (new_path);

  if (retval < 0) {
    return -errno;
  }
  return 0;
}

int nopfs_fuse_rmdir (const char *path)
{
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  return rmdir (new_path);
}

int nopfs_fuse_rename (const char *path, const char *to)
{
  char * new_path = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", path);

  char * new_path_to = malloc (sizeof (path) + 30);
  sprintf (new_path, "back%s", to);

  return rename (new_path, new_path_to);
}

int nopfs_fuse_link (const char *path, const char *to)
{
  return -EACCES;
}

int nopfs_fuse_setxattr (const char *path, const char *name, const char *value, size_t size, int position)
{
  return 0;
}

int nopfs_fuse_getxattr (const char *path, const char *name, char *buf, size_t size)
{
  return -ENOTSUP;
}

int nopfs_fuse_listxattr (const char *path, char *buf, size_t size)
{
  return -ENOTSUP;
}

int nopfs_fuse_removexattr (const char *path, const char *name)
{
  return -ENOTSUP;
}

int nopfs_fuse_chmod (const char *path, mode_t mode)
{
  return 0;
}

int nopfs_fuse_chown (const char *path, uid_t uid, gid_t gid)
{
  return 0;
}

int nopfs_fuse_utimens (const char *path, const struct timespec tv[2])
{
  //  char * new_path = malloc (sizeof (path) + 30);
  //  sprintf (new_path, "back%s", path);

  //  return utimes (new_path, tv);
  return 0;
}

void nopfs_fuse_destroy (void * v)
{
  return;
}
