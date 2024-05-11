export BTHREAD_CONCURRENCY=8
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64/
nohup ./build/test_pool conf/gflags.conf &> test_pool.log &
#nohup ./build/test_abi conf/gflags.conf &> test_abi.log &