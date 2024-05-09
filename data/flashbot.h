#pragma once
#include "util/common.h"
#include <brpc/controller.h>
#include <brpc/channel.h>
#include "data/client.h"
class FlashBotClient : public Singleton<FlashBotClient> {
public:
    int init(ClientBase* client);
    int send_bundles(const std::vector<std::string>& raw_txs, std::string& uid);
private:
    brpc::Channel _channel;
    ClientBase* _client;
    std::atomic<uint32_t> _id;
};
