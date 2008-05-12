#ifndef _PFS_FUSE_H
#define _PFS_FUSE_H

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "../global.h"

int pfs_fuse_getattr (const char *path, struct stat *stbuf);
int pfs_fuse_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
		      off_t offset, struct fuse_file_info *fi);

int pfs_fuse_open (const char *path, struct fuse_file_info *fi);
int pfs_fuse_read (const char *path, char *buf, size_t size, off_t offset,
		   struct fuse_file_info *fi);
int pfs_fuse_write (const char *path, const char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi);
int pfs_fuse_fsync (const char *path, int param, struct fuse_file_info *fi);
int pfs_fuse_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi);
int pfs_fuse_create (const char *path, mode_t mode, struct fuse_file_info *fi);
int pfs_fuse_release (const char *path, struct fuse_file_info *fi);
int pfs_fuse_truncate (const char *path, off_t offset);

int pfs_fuse_mkdir (const char *path, mode_t mode);
int pfs_fuse_unlink (const char *path);
int pfs_fuse_rmdir (const char *path);

int pfs_fuse_rename (const char *path, const char *to);

int pfs_fuse_link (const char *path, const char *to);

int pfs_fuse_setxattr (const char *path, const char *name, const char *value, size_t size, int position);
int pfs_fuse_getxattr (const char *path, const char *name, char *buf, size_t size);
int pfs_fuse_listxattr (const char *path, char *buf, size_t size);
int pfs_fuse_removexattr (const char *path, const char *name);

int pfs_fuse_chmod (const char *path, mode_t mode);
int pfs_fuse_chown (const char *path, uid_t uid, gid_t gid);

int pfs_fuse_statfs (const char *path, struct statvfs *buf);
int pfs_fuse_utimens (const char *path, const struct timespec tv[2]);
int pfs_fuse_chmod (const char *path, mode_t mode);

void pfs_fuse_destroy (void * v);

#endif
