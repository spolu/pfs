CC = gcc
CFLAGS = -Wall -g -std=gnu99
MAKE = make

LIBPFS = libpfs
PFSD = pfsd
UTILS = utils
BENCH_LFS = bench/lfs

all:
	$(MAKE) -C $(PFSD)
	$(MAKE) -C $(UTILS)
	$(MAKE) -C $(BENCH_LFS)

clean:
	@$(MAKE) -C $(PFSD) clean
	@$(MAKE) -C $(UTILS) clean
	@$(MAKE) -C $(BENCH_LFS) clean
	@rm -f *~