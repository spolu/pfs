CC = gcc
CFLAGS = -Wall -std=gnu99 -DPFS_PTHREAD_LOCK

LIB_SRC = io.c debug.c hashtable.c lock.c

lib_obj = $(LIB_SRC:.c=.o)

lib: $(lib_obj)

all: lib

clean: 
	@rm -f *~ *.o