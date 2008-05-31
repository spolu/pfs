#!/bin/sh

mount -u pfs
./pfsd back pfs -f -o volname=pFS -o default_permissions -o local

