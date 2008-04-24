#!/bin/sh

fusermount -u pfs
./pfsd back/ pfs -f

