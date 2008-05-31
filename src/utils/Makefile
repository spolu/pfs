CC = gcc
CFLAGS = -Wall -g -std=gnu99
MAKE = make

INIT_SRC = pfs_init.c
ADD_GRP_SRC = pfs_add_grp.c

TARGET_INIT = pfs_init
TARGET_ADD_GRP = pfs_add_grp

init_obj = $(INIT_SRC:.c=.o)
add_grp_obj = $(ADD_GRP_SRC:.c=.o)

pfs_init: $(init_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_INIT) $(init_obj) ../../lib/libpfs.a

pfs_add_grp: $(add_grp_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_ADD_GRP) $(add_grp_obj) ../../lib/libpfs.a

all: pfs_init pfs_add_grp

clean: 
	@rm -f ../../bin/$(TARGET_INIT)
	@rm -f ../../bin/$(TARGET_ADD_GRP)
	@rm -f *.o *~
