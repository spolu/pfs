#!/bin/sh

umount test
#./nopfs_fuse ./fuse -o volname=test -o nolocalcaches -f
./nopfs_fuse fuse -f

