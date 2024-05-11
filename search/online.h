#pragma once
#include "util/common.h"
#include "search/search_result.h"
#include "data/tx_pool.h"
#include "search/pool_manager.h"
class OnlineSearch {
public:
    OnlineSearch() {
        _pools_map.init(1);
        _pools_reverse_map.init(1);
        _pools_address_map.init(1);
        _pool_direction.init(1);
        _compute_path_cnt = 0;
        _visited_set.init(1);
    }
    ~OnlineSearch() {
        for (auto item:_pools_address_map) {
            delete item.second;
        }
    }
    void search(std::shared_ptr<Transaction> tx, const std::vector<LogEntry>& logs);
    void dfs(uint32_t cur_token, uint32_t len);
private:
    void dfs_impl(PoolBase* pool, uint32_t cur_token, uint32_t len, bool direction);
    void sandwich();
    void compute();
    butil::FlatMap<uint32_t, std::set<PoolBase*, PoolCmp>> _pools_map;
    butil::FlatMap<uint32_t, std::set<PoolBase*, PoolReverseCmp>> _pools_reverse_map;
    butil::FlatMap<Address, PoolBase*, std::hash<Address>> _pools_address_map;
    std::shared_ptr<Transaction> _tx;
    int _compute_path_cnt;
    butil::FlatMap<PoolBase*, int> _pool_direction;
    // for dfs
    butil::FlatSet<Address, std::hash<Address>> _visited_set;
    std::vector<bool> _direction;
    std::vector<PoolBase*> _path;
    uint32_t _start_token;
    int _len_limit;
};
