#!/bin/sh

mount -u pfs2
rm -Rf back2
rm -Rf pfs2
mkdir back2
mkdir pfs2
./pfs_init back2 spolu imac2

