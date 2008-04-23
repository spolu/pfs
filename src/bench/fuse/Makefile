CC = gcc
CFLAGS = -Wall -std=gnu99 -L/usr/local/lib -I/usr/local/include

NOPFS_FUSE_SRC = stub.c nopfs.c

TARGET_NOPFS_FUSE = nopfs_fuse

nopfs_fuse_obj = $(NOPFS_FUSE_SRC:.c=.o)

nopfs_fuse: $(nopfs_fuse_obj)
	$(CC) $(CFLAGS) -o $(TARGET_NOPFS_FUSE) -lfuse -lcrypto $(nopfs_fuse_obj)

clean: 
	@rm -f $(TARGET_NOPFS_FUSE) *.o *~
