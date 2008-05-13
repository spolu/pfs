#include "fuse_stub.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>


int pfs_fuse_getattr (const char *path, struct stat *stbuf)
{
  int retval;

  retval = pfs_stat (pfs, path, stbuf);
  return retval;
}


int pfs_fuse_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi)
{
  int i;

  char ** dir_entry = pfs_readdir (pfs, path);
  if (dir_entry == NULL)
    return -ENOENT;
  for (i = 0; dir_entry[i] != NULL; i ++) {
    filler (buf, dir_entry[i], NULL, 0);
    free (dir_entry[i]);
  }
  free (dir_entry);
  return 0;
}


int pfs_fuse_open (const char *path, struct fuse_file_info *fi)
{
  int fd;

  if ((fd = pfs_open (pfs, path, fi->flags, 0)) < 0) {
    return fd;
  }
  fi->fh = fd;
  return 0;
}

int pfs_fuse_read (const char *path, char *buf, size_t size, off_t offset,
		   struct fuse_file_info *fi)
{
  return pfs_pread (pfs, (int) fi->fh, buf, size, offset);
}

int pfs_fuse_write (const char *path, const char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
  return pfs_pwrite (pfs, (int) fi->fh, buf, size, offset);
}

int pfs_fuse_fsync (const char *path, int param, struct fuse_file_info *fi)
{
  return pfs_fsync (pfs, (int) fi->fh);
}

int pfs_fuse_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
  return pfs_ftruncate (pfs, (int) fi->fh, offset);
}

int pfs_fuse_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
  int fd;
  
  fi->flags |= O_CREAT;
  if ((fd = pfs_open (pfs, path, fi->flags, mode)) < 0) {
    return fd;
  }
  fi->fh = fd;
  return 0;
}

int pfs_fuse_release (const char *path, struct fuse_file_info *fi)
{
  return pfs_close (pfs, (int) fi->fh);
}

int pfs_fuse_truncate (const char *path, off_t offset)
{
  return pfs_truncate (pfs, path, offset);
}


int pfs_fuse_mkdir (const char *path, mode_t mode)
{
  return pfs_mkdir (pfs, path, mode);
}

int pfs_fuse_unlink (const char *path)
{
  return pfs_unlink (pfs, path);
}

int pfs_fuse_rmdir (const char *path)
{
  return pfs_rmdir (pfs, path);
}

int pfs_fuse_rename (const char *path, const char *to)
{
  return pfs_rename (pfs, path, to);
}

int pfs_fuse_symlink (const char *to, const char *path)
{
  return pfs_symlink (pfs, path, to);
}

int pfs_fuse_readlink (const char *path, char *buf, size_t bufsize)
{
  return pfs_readlink (pfs, path, buf, bufsize);
}

int pfs_fuse_link (const char *to, const char *path)
{
  return pfs_link (pfs, path, to);
}

int pfs_fuse_chmod (const char *path, mode_t mode)
{
  return pfs_chmod (pfs, path, mode);
}

int pfs_fuse_chown (const char *path, uid_t uid, gid_t gid)
{
  return 0;
}

int pfs_fuse_statfs (const char *path, struct statvfs *buf)
{
  return pfs_statfs (pfs, buf);  
}

int pfs_fuse_utimens (const char *path, const struct timespec tv[2])
{
  return pfs_utimens (pfs, path, tv);
}

void pfs_fuse_destroy (void * v)
{
  pfs_destroy (pfs);
  return;
}
