#pragma once
#include "util/common.h"
#include "search/search_result.h"
#include "data/tx_pool.h"
class OnlineSearch {
public:
    OnlineSearch() {
        _pools_map.init(1);
        _pools_reverse_map.init(1);
        _pools.init(1);
        _pools_address_map.init(1);
    }
    ~OnlineSearch() {
        for (auto p:_pools) {
            delete p.second;
        }
    }
    void search(std::shared_ptr<Transaction> tx, const std::vector<LogEntry>& logs);
    void dfs(uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len,
            std::vector<uint32_t>& path, std::vector<bool>& direction);
private:
    void dfs_impl(uint32_t pool_index, uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len,
         std::vector<uint32_t>& path, std::vector<bool>& direction, bool cur_direction);
    void sandwich();
    void compute(const std::vector<uint32_t>& path, const std::vector<bool>& direction);
    butil::FlatMap<uint32_t, butil::FlatMap<uint32_t, std::vector<uint32_t>>> _pools_map;
    butil::FlatMap<uint32_t, butil::FlatMap<uint32_t, std::vector<uint32_t>>> _pools_reverse_map;
    butil::FlatMap<uint32_t, PoolBase*> _pools;
    butil::FlatMap<Address, uint32_t, std::hash<Address>> _pools_address_map;
    std::shared_ptr<Transaction> _tx;
    int _compute_path_cnt = 0;
    butil::FlatMap<uint32_t, int> _pool_direction;
};
