#!/bin/sh

umount pfs
./pfsd ~spolu/.pfs/ ./pfs -f
#./pfs_fuse /Users/spolu/.pfs1/ ./purple1 -o volname=purple1 -f

