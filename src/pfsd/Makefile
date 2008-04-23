CC = gcc
CFLAGS = -Wall -g -std=gnu99 -L/usr/local/lib -I/usr/local/include
MAKE = make

FUSE = fuse
LIBPFS = ../libpfs
PFSD_SRC = pfsd.c
FUSE_SRC = fuse/fuse_stub.c

pfsd_obj = $(PFSD_SRC:.c=.o)
fuse_obj = $(FUSE_SRC:.c=.o)

TARGET_PFSD = pfsd

pfsd: $(pfsd_obj) libpfs
	$(MAKE) -C $(FUSE)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_PFSD) -lfuse -lcrypto $(pfsd_obj) $(fuse_obj) ../../lib/libpfs.a 

libpfs:
	$(MAKE) -C $(LIBPFS)

clean: 
	@rm -f ../../bin/$(TARGET_PFSD) *.o *~ 
	@$(MAKE) -C $(FUSE) clean
	@$(MAKE) -C $(LIBPFS) clean
