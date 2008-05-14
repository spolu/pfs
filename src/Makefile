CC = gcc
CFLAGS = -Wall -g -std=gnu99
MAKE = make

LIBPFS = libpfs
PFSD = pfsd
UTILS = utils
BENCH_LFS = bench/lfs
FUSE_NOPFS = bench/fuse

all:
	$(MAKE) -C $(PFSD)
	$(MAKE) -C $(UTILS)
	$(MAKE) -C $(BENCH_LFS)
	$(MAKE) -C $(FUSE_NOPFS)

clean:
	@$(MAKE) -C $(PFSD) clean
	@$(MAKE) -C $(UTILS) clean
	@$(MAKE) -C $(BENCH_LFS) clean
	@$(MAKE) -C $(FUSE_NOPFS) clean
	@rm -f *~ cscope.out