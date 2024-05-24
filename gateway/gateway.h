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
    void _on_head();
    int gen_tx(const SearchResult& result, std::string& raw_tx, std::shared_ptr<Transaction> resource_tx);
    static void* wrap_send_bundles(void* arg);
private:
    // unique account current, only one bundle sent in a block
    //uint256_t _profit;
    std::vector<FlashBotClient*> _bundle_clients;
    ClientBase* _client;
    uint64_t _nonce;
    Address _from;
    bthread_mutex_t _mutex;
    SearchResult _result;
};