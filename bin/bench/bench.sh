#!/bin/sh

echo "bench small 1"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 >> bench.lfs_small
echo "bench small 2"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 >> bench.lfs_small
echo "bench small 3"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 >> bench.lfs_small
echo "bench small 4"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 >> bench.lfs_small
echo "bench small 5"
/home/spolu/pfs/bin/bench/lfs_small 10000 4096 >> bench.lfs_small

echo "bench large 1"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 >> bench.lfs_large
echo "bench large 2"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 >> bench.lfs_large
echo "bench large 3"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 >> bench.lfs_large
echo "bench large 4"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 >> bench.lfs_large
echo "bench large 5"
/home/spolu/pfs/bin/bench/lfs_large 30000 4096 >> bench.lfs_large
