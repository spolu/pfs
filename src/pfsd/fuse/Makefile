CC = gcc
CFLAGS = -Wall -std=gnu99 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse

FUSE_SRC = fuse_stub.c

fuse_obj = $(FUSE_SRC:.c=.o)

fuse: $(fuse_obj)

all: fuse

clean: 
	@rm -f *~ *.o
