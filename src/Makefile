CC = gcc
CFLAGS = -Wall -g -std=gnu99
MAKE = make

LIBPFS = libpfs
PFSD = pfsd
UTILS = utils
BENCH = bench

all:
	$(MAKE) -C $(PFSD)
	$(MAKE) -C $(UTILS)

clean:
	@$(MAKE) -C $(PFSD) clean
	@$(MAKE) -C $(UTILS) clean
	@rm -f *~