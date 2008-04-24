#include "stub.h"

struct fuse_operations nopfs_operations = {
  .getattr = nopfs_fuse_getattr,
  .readdir = nopfs_fuse_readdir,
  .open = nopfs_fuse_open,
  .read = nopfs_fuse_read,
  .write = nopfs_fuse_write,
  .fsync = nopfs_fuse_fsync,
  .ftruncate = nopfs_fuse_ftruncate,
  .create = nopfs_fuse_create,
  .release = nopfs_fuse_release,
  .truncate = nopfs_fuse_truncate,
  .mkdir = nopfs_fuse_mkdir,
  .unlink = nopfs_fuse_unlink,
  .rmdir = nopfs_fuse_rmdir,
  .rename = nopfs_fuse_rename,
  .link = nopfs_fuse_link,
  .setxattr = nopfs_fuse_setxattr,
  .getxattr = nopfs_fuse_getxattr,
  .listxattr = nopfs_fuse_listxattr,
  .removexattr = nopfs_fuse_removexattr,
  .chmod = nopfs_fuse_chmod,
  .chown = nopfs_fuse_chown,
  .utimens = nopfs_fuse_utimens,
  .destroy = nopfs_fuse_destroy,
};

