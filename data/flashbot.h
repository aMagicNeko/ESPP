#pragma once
#include "util/common.h"
#include <brpc/controller.h>
#include <brpc/channel.h>
#include "data/client.h"
class FlashBotClient {
public:
    int init(ClientBase* client, std::string relay_url);
    int send_bundles(const std::vector<std::string>& raw_txs);
    void on_head();
private:
    brpc::Channel _channel;
    ClientBase* _client;
    std::atomic<uint32_t> _id;
    bthread_mutex_t _mutex;
    std::string _relay_url;
    std::string _uid;
};
