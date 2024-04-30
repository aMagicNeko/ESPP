#pragma once
#include "util/common.h"
#include "util/type.h"
#include "search/search_result.h"
#include "data/tx_pool.h"
class GateWay : public Singleton<GateWay> {
public:
    void notice_search_offline_result(const SearchResult& result);
    void notice_search_online_result(const SearchResult& result, std::shared_ptr<Transaction> tx);
private:
};