CC = gcc
CFLAGS = -Wall -g -std=gnu99 -L/usr/local/lib -I/usr/local/include

FUSE_SRC = fuse_stub.c

fuse_obj = $(FUSE_SRC:.c=.o)

fuse: $(fuse_obj)

all: fuse

clean: 
	@rm -f *~ *.o
