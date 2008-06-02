CC = gcc
CFLAGS = -Wall -g -std=gnu99
MAKE = make

INIT_SRC = pfs_init.c
ADD_GRP_SRC = pfs_add_grp.c
ADD_SD_SRC = pfs_add_sd.c
LIST_SD_SRC = pfs_list_sd.c

TARGET_INIT = pfs_init
TARGET_ADD_GRP = pfs_add_grp
TARGET_ADD_SD = pfs_add_sd
TARGET_LIST_SD = pfs_list_sd

init_obj = $(INIT_SRC:.c=.o)
add_grp_obj = $(ADD_GRP_SRC:.c=.o)
add_sd_obj = $(ADD_SD_SRC:.c=.o)
list_sd_obj = $(LIST_SD_SRC:.c=.o)

pfs_init: $(init_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_INIT) $(init_obj) ../../lib/libpfs.a

pfs_add_grp: $(add_grp_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_ADD_GRP) $(add_grp_obj) ../../lib/libpfs.a

pfs_add_sd: $(add_sd_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_ADD_SD) $(add_sd_obj) ../../lib/libpfs.a

pfs_list_sd: $(list_sd_obj)
	$(CC) $(CFLAGS) -o ../../bin/$(TARGET_LIST_SD) $(list_sd_obj) ../../lib/libpfs.a

all: pfs_init pfs_add_grp pfs_add_sd pfs_list_sd

clean: 
	@rm -f ../../bin/$(TARGET_INIT)
	@rm -f ../../bin/$(TARGET_ADD_GRP)
	@rm -f ../../bin/$(TARGET_ADD_SD)
	@rm -f *.o *~
