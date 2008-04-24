#!/bin/sh

echo "bench small fsync 1"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 1 >> bench.lfs_small_fsync
echo "bench small fsync 2"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 1 >> bench.lfs_small_fsync
echo "bench small fsync 3"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 1 >> bench.lfs_small_fsync
echo "bench small fsync 4"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 1 >> bench.lfs_small_fsync
echo "bench small fsync 5"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 1 >> bench.lfs_small_fsync

echo "bench small no_fsync 1"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 0 >> bench.lfs_small_no_fsync
echo "bench small no_fsync 2"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 0 >> bench.lfs_small_no_fsync
echo "bench small no_fsync 3"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 0 >> bench.lfs_small_no_fsync
echo "bench small no_fsync 4"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 0 >> bench.lfs_small_no_fsync
echo "bench small no_fsync 5"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 0 >> bench.lfs_small_no_fsync


echo "bench large fsync 1"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 1 >> bench.lfs_large_fsync
echo "bench large fsync 2"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 1 >> bench.lfs_large_fsync
echo "bench large fsync 3"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 1 >> bench.lfs_large_fsync
echo "bench large fsync 4"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 1 >> bench.lfs_large_fsync
echo "bench large fsync 5"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 1 >> bench.lfs_large_fsync

echo "bench large no_fsync 1"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 0 >> bench.lfs_large_no_fsync
echo "bench large no_fsync 2"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 0 >> bench.lfs_large_no_fsync
echo "bench large no_fsync 3"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 0 >> bench.lfs_large_no_fsync
echo "bench large no_fsync 4"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 0 >> bench.lfs_large_no_fsync
echo "bench large no_fsync 5"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 0 >> bench.lfs_large_no_fsync
