#include "data/uniswapv2.h"

#include <stdio.h>
#include "data/websocket.h"
#include "data/secure_websocket.h"
#include "data/tx_pool.h"
#include "search/pool_manager.h"
#include "data/ipc.h"
#include "gateway/gateway.h"
#include <ethash/keccak.hpp>
#include "search/online.h"
DEFINE_string(host, "45.250.254.213", "RPC host");
DEFINE_string(port, "8546", "RPC port");
DEFINE_string(path, "/", "RPC path");
DEFINE_int32(ssl, 0, "is SSL");
DEFINE_string(ipc_url, "", "url of ipc socket");

//std::string stringToHex(const std::string& input) ;
//std::vector<uint8_t> sign_message(const std::vector<uint8_t>& hash, const std::vector<uint8_t>& private_key);
int main (int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s flagfile\n", argv[0]);
        return 1;
    }
    LOG(INFO) << "set gflags from file: " << google::SetCommandLineOption("flagfile", argv[1]);
    
    auto client = new IpcClient;
    if (client->connect(FLAGS_ipc_url) != 0) {
        return 0;
    }
    client->start_listen();
    /*
    #include "data/request.h"
    std::string message = "";

    const std::string address = "0xa22cf23D58977639e05B45A3Db7dF6D4a91eb892";
    std::string sig;
    ethash::hash256 hash = ethash::keccak256(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    DBytes data(hash.bytes, 32);
    message = data.to_string();
    message = std::string("\x19") + "Ethereum Signed Message:\n" + std::to_string(message.size()) + message;
    hash = ethash::keccak256(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    data = DBytes(hash.bytes, 32);
    DBytes pri("979a2f5e87e873629eabd0e3d4f82020d279375c8b2ae588fff6abba98e1f192");
    auto sign = sign_message(data._data, pri._data);
    sig = DBytes(sign.data(), sign.size()).to_string();
    //request_sign_data(client, address, data.to_string(), sig);
    LOG(INFO) << "sig:" << sig;
    sleep(100000);
    */
    usleep(1000);
    TxPool::instance()->init(client);
    PoolManager::instance()->init(client);
    GateWay::instance()->init(client);
    client->subscribe_headers();
    client->subscribe_transactions();
    //OnlineSearch::start_offline_search_thread(1);
    while (1) {
        sleep(1);
    }
}