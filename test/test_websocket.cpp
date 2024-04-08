#include <stdio.h>
#include "data/websocket.h"
#include "data/secure_websocket.h"
#include "data/tx_pool.h"
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
    if (!FLAGS_ssl) {
        Websocket ws;
        ws.connect(FLAGS_host, FLAGS_port, FLAGS_path);
        ws.start_listen();
        //int id = 1;
        while (1) {
            sleep(3);
            //json j;
            // Request
            //j["jsonrpc"] = "2.0";
            //j["method"] = "eth_blockNumber";
            //j["params"] = json::array();
            //j["id"] = id;
            //++id;
            //ws.write(j);
        }
    }
    else {
        SecureWebsocket ws;
        ws.connect(FLAGS_host, FLAGS_port, FLAGS_path);
        ws.start_listen();
        int id = 1;
        while (1) {
            sleep(3);
            json j;
            // Request
            j["jsonrpc"] = "2.0";
            j["method"] = "eth_blockNumber";
            j["params"] = json::array();
            j["id"] = id++;
            ws.write(j);
        }
    }

    return 0;
}