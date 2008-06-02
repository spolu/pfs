#!/bin/sh

mount -u pfs2
./pfsd 10099 back2 pfs2 -f -o volname=pFS2 -o defer_permissions -o nolocalcaches

