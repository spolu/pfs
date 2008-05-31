CC = gcc
CFLAGS = -Wall -std=gnu99 -DPFS_PTHREAD_LOCK -D_FILE_OFFSET_BITS=64 -DDEBUG

LIB = lib
LIB_SRC = lib/io.c lib/debug.c lib/hashtable.c lib/lock.c lib/sha1.c
PFS_SRC = entry.c instance.c file.c pfs.c updt.c dir_cache.c path.c group.c

pfs_obj = $(PFS_SRC:.c=.o)
lib_obj = $(LIB_SRC:.c=.o)

TARGET_LIBPFS = libpfs.a


libpfs: $(pfs_obj)
	make -C $(LIB)
	ar r ../../lib/$(TARGET_LIBPFS) $(pfs_obj) $(lib_obj)

all: libpfs

clean: 
	@rm -f ../../lib/$(TARGET_LIBPFS) *.o *~ cscope.out
	@make -C $(LIB) clean