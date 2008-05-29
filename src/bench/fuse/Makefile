CC = gcc
CFLAGS = -Wall -std=gnu99 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse

NOPFS_FUSE_SRC = stub.c nopfs.c

TARGET_NOPFS_FUSE = nopfs_fuse

nopfs_fuse_obj = $(NOPFS_FUSE_SRC:.c=.o)

nopfs_fuse: $(nopfs_fuse_obj)
	$(CC) $(CFLAGS) -o ../../../bin/bench/fuse/$(TARGET_NOPFS_FUSE) -lfuse -lcrypto -ldl $(nopfs_fuse_obj)

clean: 
	@rm -f ../../../bin/bench/fuse/$(TARGET_NOPFS_FUSE) *.o *~
