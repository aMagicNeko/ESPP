#include "search/online.h"
#include "search/pool_manager.h"
#include "gateway/gateway.h"
DEFINE_int32(online_search_max_length, 5, "max path length of online search");
void OnlineSearch::search(std::shared_ptr<Transaction> tx, const std::vector<LogEntry>& logs) {
    _tx = tx;
    for (auto& log: logs) {
        if (!_pools_address_map.seek(log.address)) {
            auto p = PoolManager::instance()->_pools_address_map.seek(log.address);
            if (p) {
                uint32_t i = *p;
                PoolBase* pool = PoolManager::instance()->_pools[i]->get_copy();
                _pools_address_map.insert(log.address, i);
                uint32_t token1 = pool->token1;
                uint32_t token2 = pool->token2;
                _pools[i] = pool;
                auto pp = _pools_map.seek(token1);
                if (pp == 0) {
                    pp = _pools_map.insert(token1, butil::FlatMap<uint32_t, std::vector<uint32_t>>());
                    pp->init(1);
                }
                auto ppp = pp->seek(token2);
                if (ppp == 0) {
                    ppp = pp->insert(token2, std::vector<uint32_t>());
                }
                ppp->push_back(i);

                pp = _pools_reverse_map.seek(token2);
                if (pp == 0) {
                    pp = _pools_reverse_map.insert(token2, butil::FlatMap<uint32_t, std::vector<uint32_t>>());
                    pp->init(1);
                }
                ppp = pp->seek(token1);
                if (ppp == 0) {
                    ppp = pp->insert(token1, std::vector<uint32_t>());
                }
                ppp->push_back(i);
                pool->on_event(log, 1);
            }
        }
        else {
            _pools[_pools_address_map[log.address]]->on_event(log, 1);
        }
    }
    for (auto item : _pools) {
        std::vector<uint32_t> path;
        path.push_back(item.first);
        butil::FlatSet<uint32_t> visited_set;
        visited_set.init(1);
        visited_set.insert(item.first);

        uint32_t start_token = item.second->token1;
        std::vector<bool> direction;
        direction.push_back(1);
        dfs(start_token, visited_set, item.second->token2, 1, path, direction);

        direction[0] = 0;
        start_token = item.second->token2;
        dfs(start_token, visited_set, item.second->token1, 1, path, direction);
    }
}

inline void OnlineSearch::dfs_impl(uint32_t pool_index, uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len,
         std::vector<uint32_t>& path, std::vector<bool>& direction, bool cur_direction) {
    if (visited_set.seek(pool_index)) {
        return;
    }
    visited_set.insert(pool_index);
    path.push_back(pool_index);
    direction.push_back(cur_direction);
    if (cur_token == start_token) {
        compute(path, direction);
    }
    else {
        dfs(start_token, visited_set, cur_token, len + 1, path, direction);
    }
    direction.pop_back();
    path.pop_back();
    visited_set.erase(pool_index);
}

void OnlineSearch::dfs(uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len,
         std::vector<uint32_t>& path, std::vector<bool>& direction) {
    if (len > FLAGS_online_search_max_length) {
        return;
    }
    auto item = _pools_map.seek(cur_token);
    if (item) {
        for (auto p:*item) {
            for (uint32_t pool : p.second) {
                dfs_impl(pool, start_token, visited_set, p.first, len + 1, path, direction, 1);
            }
        }
    }
    item = _pools_reverse_map.seek(cur_token);
    if (item) {
        for (auto p:*item) {
            for (uint32_t pool : p.second) {
                dfs_impl(pool, start_token, visited_set, p.first, len + 1, path, direction, 0);
            }
        }
    }

    for (auto p : PoolManager::instance()->_pools_map[cur_token]) {
        for (uint32_t pool : p.second) {
            dfs_impl(pool, start_token, visited_set, p.first, len + 1, path, direction, 1);
        }
    }
    for (auto p : PoolManager::instance()->_pools_reverse_map[cur_token]) {
        for (uint32_t pool : p.second) {
            dfs_impl(pool, start_token, visited_set, p.first, len + 1, path, direction, 0);
        }
    }
}

SearchResult compute_impl(const std::vector<PoolBase*>& path, const std::vector<bool>& direction, const std::vector<uint32_t>& pool_index);

void OnlineSearch::compute(const std::vector<uint32_t>& path, const std::vector<bool>& direction) {
    std::vector<PoolBase*> pools;
    for (uint32_t i : path) {
        auto p = _pools.seek(i);
        PoolBase* pool = NULL;
        if (p) {
            pool = (*p)->get_copy();
        }
        else {
            pool = PoolManager::instance()->_pools[i]->get_copy();
        }
        pools.push_back(pool);
    }
    SearchResult result = compute_impl(pools, direction, path);
    GateWay::instance()->notice_search_online_result(result, _tx);
}
