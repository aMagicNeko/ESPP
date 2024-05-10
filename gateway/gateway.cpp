#include "gateway/gateway.h"
#include "data/flashbot.h"
const Address WETH_ADDR("0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2");
struct BundleArg {
    bool online;
    SearchResult result;
    std::shared_ptr<Transaction> tx;
};

void* sent_bundle_wrap(void* args) {
    BundleArg* arg = static_cast<BundleArg*> (args);

    delete arg;
    return NULL;
}

void GateWay::notice_search_offline_result(const SearchResult& result) {
    uint256_t profit = result.eth_out;
    if (profit > _profit) {
        _profit = profit;
        bthread_t bid;
        auto arg = new BundleArg;
        arg->online = false;
        arg->result = result;
        bthread_start_background(&bid, NULL, sent_bundle_wrap, arg);
    }
}

void GateWay::notice_search_online_result(const SearchResult& result, std::shared_ptr<Transaction> tx) {
    uint256_t profit = result.eth_out;
    if (profit > _profit) {
        _profit = profit;
        bthread_t bid;
        auto arg = new BundleArg;
        arg->online = true;
        arg->result = result;
        arg->tx = tx;
        bthread_start_background(&bid, NULL, sent_bundle_wrap, NULL);
    }
}

void GateWay::on_head() {
    _uid = "";
    _profit = 0;
}