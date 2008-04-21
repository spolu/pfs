CC = gcc
CFLAGS = -Wall -g -std=gnu99 -L/usr/local/lib -I/usr/local/include
MAKE = make

LFS_LARGE_SRC = performance.large.c
LFS_SMALL_SRC = performance.small.c

TARGET_LFS_LARGE = lfs_large
TARGET_LFS_SMALL = lfs_small

lfs:
	$(CC) $(CFLAGS) -o ../../../bin/bench/$(TARGET_LFS_LARGE) $(LFS_LARGE_SRC) 
	$(CC) $(CFLAGS) -o ../../../bin/bench/$(TARGET_LFS_SMALL) $(LFS_SMALL_SRC) 

clean: 
	@rm -f *.o *~
	@rm -f ../../../bin/bench/$(TARGET_LFS_LARGE) 
	@rm -f ../../../bin/bench/$(TARGET_LFS_SMALL) 
