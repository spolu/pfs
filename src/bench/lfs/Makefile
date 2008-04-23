CC = gcc
CFLAGS = -Wall -O3 -std=gnu99 -L/usr/local/lib -I/usr/local/include
MAKE = make

LFS_LARGE_SRC = performance.large.c
LFS_SMALL_SRC = performance.small.c
LFS_SMALL_DIRECT_SRC = direct.pfs.small.c

TARGET_LFS_LARGE = lfs_large
TARGET_LFS_SMALL = lfs_small
TARGET_LFS_SMALL_DIRECT = lfs_small_direct_pfs

lfs:
	$(CC) $(CFLAGS) -o ../../../bin/bench/$(TARGET_LFS_LARGE) $(LFS_LARGE_SRC) 
	$(CC) $(CFLAGS) -o ../../../bin/bench/$(TARGET_LFS_SMALL) $(LFS_SMALL_SRC) 
	$(CC) $(CFLAGS) -o ../../../bin/bench/$(TARGET_LFS_SMALL_DIRECT) -lcrypto ../../../lib/libpfs.a $(LFS_SMALL_DIRECT_SRC) 

clean: 
	@rm -f *.o *~
	@rm -f ../../../bin/bench/$(TARGET_LFS_LARGE) 
	@rm -f ../../../bin/bench/$(TARGET_LFS_SMALL) 
	@rm -f ../../../bin/bench/$(TARGET_LFS_SMALL_DIRECT) 
