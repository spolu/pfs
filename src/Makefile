CC = cc
CFLAGS = -Wall -g -std=gnu99 -DPFS_PTHREAD_LOCK\
	-D_FILE_OFFSET_BITS=64 -L/usr/local/lib -I/usr/local/include

LIB_SRC = libpfs/lib/io.c libpfs/lib/debug.c libpfs/lib/hashtable.c libpfs/lib/lock.c
PFS_SRC = libpfs/entry.c libpfs/instance.c libpfs/file.c libpfs/pfs.c libpfs/updt.c \
	  libpfs/dir_cache.c libpfs/path.c
PFSD_SRC = pfsd/pfsd.c
FUSE_SRC = pfsd/fuse/fuse_stub.c

pfs_obj = $(PFS_SRC:.c=.o)
lib_obj = $(LIB_SRC:.c=.o)
pfsd_obj = $(PFSD_SRC:.c=.o)
fuse_obj = $(FUSE_SRC:.c=.o)

TARGET_LIBPFS = libpfs.a
TARGET_PFSD = pfsd

libpfs:
	cd libpfs
	$(CC) $(CFLAGS) -c $(PFS_SRC)
	cd lib
	$(CC) $(CFLAGS) -c $(LIB_SRC)	
	ar r ../lib/$(TARGET_LIBPFS) $(pfs_obj) $(lib_obj)

pfsd: $(pfsd_obj) $(fuse_obj) libpfs
	$(CC) $(CFLAGS) -o ../bin/$(TARGET_PFSD) -lfuse -lcrypto \
	$(pfsd_obj) $(fuse_obj) ../lib/libpfs.a 

all: pfsd

clean:
	@rm -f *~
	@rm -f ../lib/$(TARGET_LIBPFS)
	@rm -f libpfs/*~ libpfs/*.o
	@rm -f libpfs/lib/*~ libpfs/lib/*.o
	@rm -f ../bin/$(TARGET_PFSD) 
	@rm -f pfsd/*.o pfsd/*~ 
	@rm -f pfsd/fuse/*.o pfsd/fuse/*~
