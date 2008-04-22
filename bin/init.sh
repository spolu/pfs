#!/bin/sh

umount ./pfs
rm -Rf ~spolu/.pfs
mkdir ~spolu/.pfs
./pfs_init ~spolu/.pfs/ spolu iMac

