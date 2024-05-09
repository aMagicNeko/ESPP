#include "data/uniswapv2.h"

#include <stdio.h>
#include "data/websocket.h"
#include "data/secure_websocket.h"
#include "data/tx_pool.h"
#include "search/pool_manager.h"
#include "data/ipc.h"
DEFINE_string(host, "45.250.254.213", "RPC host");
DEFINE_string(port, "8546", "RPC port");
DEFINE_string(path, "/", "RPC path");
DEFINE_int32(ssl, 0, "is SSL");
DEFINE_string(ipc_url, "", "url of ipc socket");
int main (int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s flagfile\n", argv[0]);
        return 1;
    }
    LOG(INFO) << "set gflags from file: " << google::SetCommandLineOption("flagfile", argv[1]);
    
    IpcClient client;
    if (client.connect(FLAGS_ipc_url) != 0) {
        return 0;
    }
    client.start_listen();
    usleep(1000);
    TxPool::instance()->init(&client);
    evmc::SimulateManager::instance()->start(&client);
    PoolManager::instance()->init(&client);
    client.subscribe_headers();
    client.subscribe_transactions();
    while (1) {
        sleep(1);
    }
}