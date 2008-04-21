CC = gcc
CFLAGS = -Wall -g -std=gnu99 -D_FILE_OFFSET_BITS=64 -L/usr/local/lib -I/usr/local/include

PFSD_SRC = pfsd.c

TARGET_PFSD = pfsd

pfsd_obj = $(PFSD_SRC:.c=.o)

pfsd: $(pfsd_obj) libpfs fuse/fuse_stub.o
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_PFSD) -lfuse -lcrypto $(pfsd_obj) ../../lib/libpfs.a fuse/fuse_stub.o

libpfs:
	-$(MAKE) -C ../libpfs

clean: 
	@rm -f ../../bin/$(TARGET_PFSD) *.o *~ fuse/*.o fuse/*~
	@$(MAKE) -C ../libpfs clean
