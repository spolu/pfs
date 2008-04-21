CC = gcc
CFLAGS = -Wall -g -std=gnu99 -DPFS_PTHREAD_LOCK

LIB_SRC = lib/io.c lib/debug.c lib/hashtable.c lib/lock.c
PFS_SRC = entry.c instance.c file.c pfs.c updt.c dir_cache.c path.c

pfs_obj = $(PFS_SRC:.c=.o)
lib_obj = $(LIB_SRC:.c=.o)

TARGET_LIBPFS = libpfs.a

libpfs: $(pfs_obj) $(lib_obj)
	ar r ../../lib/$(TARGET_LIBPFS) $(pfs_obj) $(lib_obj)

clean: 
	@rm -f ../../lib/$(TARGET_LIBPFS) *.o *~
	@rm -f lib/*~ lib/*.o