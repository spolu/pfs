#!/bin/sh

umount test
#./nopfs_fuse ./test -o volname=test -o nolocalcaches -f
./nopfs_fuse ./test -o volname=test -f

