#include "data/uniswapv2.h"

#include <stdio.h>
#include "data/websocket.h"
#include "data/secure_websocket.h"
#include "data/tx_pool.h"
#include "simulate/simulate_manager.h"
DEFINE_string(host, "45.250.254.213", "RPC host");
DEFINE_string(port, "8546", "RPC port");
DEFINE_string(path, "/", "RPC path");
DEFINE_int32(ssl, 0, "is SSL");
int main (int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s flagfile\n", argv[0]);
        return 1;
    }
    LOG(INFO) << "set gflags from file: " << google::SetCommandLineOption("flagfile", argv[1]);
    if (FLAGS_ssl) {
        SecureWebsocket ws;
        ws.connect(FLAGS_host, FLAGS_port, FLAGS_path);
        ws.start_listen();
        usleep(1000);
        TxPool::instance()->init(&ws);
        evmc::SimulateManager::instance()->start(&ws);
        //evmc::SimulateManager::instance()->check_simulate();
        ws.subscribe_headers();
        ws.subscribe_transactions();
        while (1) {
            sleep(1);
        } 
    }
    else {
        Websocket ws;
        ws.connect(FLAGS_host, FLAGS_port, FLAGS_path);
        ws.start_listen();
        // 等待启动起来
        usleep(1000);
        TxPool::instance()->init(&ws);
        evmc::SimulateManager::instance()->start(&ws);
        //evmc::SimulateManager::instance()->check_simulate();
        ws.subscribe_headers();
        ws.subscribe_transactions();
        while (1) {
            sleep(1);
        }
    }
    return 0;

}