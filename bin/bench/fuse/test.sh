#!/bin/sh

fusermount -u fuse
rm -Rf fuse
rm -Rf back
mkdir fuse
mkdir back
./nopfs_fuse fuse -f

