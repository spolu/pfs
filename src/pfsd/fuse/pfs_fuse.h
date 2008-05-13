#include "fuse_stub.h"

struct fuse_operations pfs_operations = {
  .getattr = pfs_fuse_getattr,
  .readdir = pfs_fuse_readdir,
  .open = pfs_fuse_open,
  .read = pfs_fuse_read,
  .write = pfs_fuse_write,
  .fsync = pfs_fuse_fsync,
  .ftruncate = pfs_fuse_ftruncate,
  .create = pfs_fuse_create,
  .release = pfs_fuse_release,
  .truncate = pfs_fuse_truncate,
  .mkdir = pfs_fuse_mkdir,
  .unlink = pfs_fuse_unlink,
  .rmdir = pfs_fuse_rmdir,
  .rename = pfs_fuse_rename,
  .symlink = pfs_fuse_symlink,
  .readlink = pfs_fuse_readlink,
  .link = pfs_fuse_link,
  //.setxattr = pfs_fuse_setxattr,
  //.getxattr = pfs_fuse_getxattr,
  //.listxattr = pfs_fuse_listxattr,
  //.removexattr = pfs_fuse_removexattr,
  .chmod = pfs_fuse_chmod,
  .chown = pfs_fuse_chown,
  .statfs = pfs_fuse_statfs,
  .utimens = pfs_fuse_utimens,
  .destroy = pfs_fuse_destroy,
};

