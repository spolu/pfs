#!/bin/sh

mount -u pfs
./pfsd 9999 back pfs -f -o volname=pFS -o defer_permissions -o nolocalcaches

