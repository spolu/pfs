#!/bin/sh

mount -u pfs
rm -Rf back
rm -Rf pfs
mkdir back
mkdir pfs
./pfs_init back spolu imac1

