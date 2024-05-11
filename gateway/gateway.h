#pragma once
#include "util/common.h"
#include "util/type.h"
#include "search/search_result.h"
#include "data/tx_pool.h"
#include "data/flashbot.h"
class GateWay : public Singleton<GateWay> {
public:
    int init(ClientBase* client);
    void notice_search_offline_result(const SearchResult& result);
    void notice_search_online_result(const SearchResult& result, std::shared_ptr<Transaction> tx);
    void on_head();
private:
    // unique account current, only one bundle sent in a block
    std::string _uid;
    uint256_t _profit;
    FlashBotClient _bundle_client;
};