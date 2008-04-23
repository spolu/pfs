CC = gcc
CFLAGS = -Wall -g -std=gnu99 -D_FILE_OFFSET_BITS=64
MAKE = make

INIT_SRC = pfs_init.c

TARGET_INIT = pfs_init

init_obj = $(INIT_SRC:.c=.o)

pfs_init: $(init_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_INIT) -lcrypto $(init_obj) ../../lib/libpfs.a

clean: 
	@rm -f ../../bin/$(TARGET_INIT)
	@rm -f *.o *~
