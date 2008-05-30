CC = gcc
CFLAGS = -Wall -std=gnu99 -D_FILE_OFFSET_BITS=64 -DPFS_PTHREAD_LOCK -I/usr/include/fuse
MAKE = make

FUSE = fuse
LIBPFS = ../libpfs
PFSD_SRC = pfsd.c log.c srv.c prop.c
FUSE_SRC = fuse/fuse_stub.c

pfsd_obj = $(PFSD_SRC:.c=.o)
fuse_obj = $(FUSE_SRC:.c=.o)

TARGET_PFSD = pfsd

pfsd: $(pfsd_obj) libpfs
	$(MAKE) -C $(FUSE)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_PFSD) -lfuse -ldl $(pfsd_obj) $(fuse_obj) ../../lib/libpfs.a 

libpfs:
	$(MAKE) -C $(LIBPFS)

clean: 
	@rm -f ../../bin/$(TARGET_PFSD) *.o *~ 
	@$(MAKE) -C $(FUSE) clean
	@$(MAKE) -C $(LIBPFS) clean
