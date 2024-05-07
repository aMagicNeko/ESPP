export BTHREAD_CONCURRENCY=8
export LD_PRELOAD=/usr/lib/gcc/aarch64-linux-gnu/11/libasan.so

nohup ./build/test_abi conf/gflags.conf > test_abi.log
#nohup ./build/test_simulate conf/gflags.conf > test_simulate.log


#curl https://mainnet.infura.io/v3/611e8f500ea14a8a93a8f9d030417830 \
#  -X POST \
#  -H "Content-Type: application/json" \
#  -d '{"jsonrpc":"2.0","method":"eth_call","params": [{"from": "0xb60e8dd61c5d32be8058bb8eb970870f07233155","to": "0x18adbc0144059be823a5d0de1d6445002f858eb5","data": "0xf30dba93000000000000000000000000000000000000000000000000000000000001512a"}, "latest"],"id":1}'