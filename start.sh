export BTHREAD_CONCURRENCY=8
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64/
nohup ./build/main conf/gflags.conf &> main.log &