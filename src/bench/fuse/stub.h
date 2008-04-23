#ifndef _NOPFS_FUSE_H
#define _NOPFS_FUSE_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64
#include <fuse.h>

int nopfs_fuse_getattr (const char *path, struct stat *stbuf);
int nopfs_fuse_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
		      off_t offset, struct fuse_file_info *fi);

int nopfs_fuse_open (const char *path, struct fuse_file_info *fi);
int nopfs_fuse_read (const char *path, char *buf, size_t size, off_t offset,
		   struct fuse_file_info *fi);
int nopfs_fuse_write (const char *path, const char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi);
int nopfs_fuse_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi);
int nopfs_fuse_create (const char *path, mode_t mode, struct fuse_file_info *fi);
int nopfs_fuse_release (const char *path, struct fuse_file_info *fi);
int nopfs_fuse_truncate (const char *path, off_t offset);

int nopfs_fuse_mkdir (const char *path, mode_t mode);
int nopfs_fuse_unlink (const char *path);
int nopfs_fuse_rmdir (const char *path);

int nopfs_fuse_rename (const char *path, const char *to);

int nopfs_fuse_link (const char *path, const char *to);

int nopfs_fuse_setxattr (const char *path, const char *name, const char *value, size_t size, int position);
int nopfs_fuse_getxattr (const char *path, const char *name, char *buf, size_t size);
int nopfs_fuse_listxattr (const char *path, char *buf, size_t size);
int nopfs_fuse_removexattr (const char *path, const char *name);

int nopfs_fuse_chmod (const char *path, mode_t mode);
int nopfs_fuse_chown (const char *path, uid_t uid, gid_t gid);

int nopfs_fuse_utimens (const char *path, const struct timespec tv[2]);

void nopfs_fuse_destroy (void * v);

#endif
