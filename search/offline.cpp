#include "search/offline.h"
#include "search/pool_manager.h"
#include "search/analytical_solution.h"
#include <deque>
void* search_thread_wrap(void* arg) {
    OfflineSearch::instance()->search();
    return NULL;
}

void OfflineSearch::start(uint32_t thread_num, uint32_t limit_length) {
    _cur_token.store(0);
    for (uint32_t i = 0; i < thread_num; ++i) {
        bthread_t bid;
        bthread_start_background(&bid, NULL, search_thread_wrap, NULL);
    }
}

void OfflineSearch::search() {
    while (true) {
        uint32_t cur = _cur_token.fetch_add(1) % PoolManager::instance()->tokens_num();
        // dfs
        butil::FlatSet<uint32_t> visited_set;
        std::vector<uint32_t> path;
        std::vector<bool> directioin;
        dfs(cur, visited_set, cur, 0, path, directioin);
    }
}

void OfflineSearch::dfs(uint32_t start_token, butil::FlatSet<uint32_t>& visited_set, uint32_t cur_token, uint32_t len,
         std::vector<uint32_t>& path, std::vector<bool>& direction) {
    if (len > _limit_length) {
        return;
    }
    for (auto p : PoolManager::instance()->_pools_map[cur_token]) {
        for (uint32_t pool : p.second) {
            if (visited_set.seek(pool)) {
                continue;
            }
            visited_set.insert(pool);
            path.push_back(pool);
            direction.push_back(1);
            if (p.first == start_token) {
                compute(path, direction);
            }
            else {
                dfs(start_token, visited_set, p.first, len + 1, path, direction);
            }
            direction.pop_back();
            path.pop_back();
            visited_set.erase(pool);
        }
    }
    for (auto p : PoolManager::instance()->_pools_reverse_map[cur_token]) {
        for (uint32_t pool : p.second) {
            if (visited_set.seek(pool)) {
                continue;
            }
            visited_set.insert(pool);
            path.push_back(pool);
            direction.push_back(0);
            if (p.first == start_token) {
                compute(path, direction);
            }
            else {
                dfs(start_token, visited_set, p.first, len + 1, path, direction);
            }
            direction.pop_back();
            path.pop_back();
            visited_set.erase(pool);
        }
    }
}

void OfflineSearch::compute(const std::vector<uint32_t>& path, const std::vector<bool>& direction) {
    LOG(INFO) << "compute start:" << path.size();
    std::vector<PoolBase*> pools;
    for (uint32_t i : path) {
        auto p = PoolManager::instance()->_pools[i]->get_copy();
        pools.push_back(p);
    }
    uint256_t accumulate_input = 0;
    uint256_t accumulate_output = 0;
    while (true) {
        uint256_t cur_boundary = (uint256_t(1) << 112);
        for (uint32_t i = 0; i < pools.size(); ++i) {
            LOG(INFO) << "tick: " << pools[i]->get_tick() << " liquidity:" << pools[i]->get_liquidit();
        }
        for (uint32_t i = 0; i < pools.size(); ++i) {
            cur_boundary = pools[i]->get_output_boundary(cur_boundary, direction[i]);
        }
        for (uint32_t i = pools.size() - 1; i >= 0; --i) {
            cur_boundary = pools[i]->compute_input(cur_boundary, direction[i]);
        }
        uint256_t token_in = get_analytical_solution(pools, direction, cur_boundary);
        assert(token_in <= cur_boundary);
        uint256_t token_out = token_in;
        for (uint32_t i = 0; i < pools.size(); ++i) {
            token_out = pools[i]->compute_output(token_out, direction[i]);
        }
        LOG(INFO) << "token in:" << token_in + accumulate_input << " token_out:" << token_out + accumulate_output;
        // to next round
        accumulate_input += cur_boundary;
        for (uint32_t i = 0; i < pools.size(); ++i) {
            cur_boundary = pools[i]->process_swap(cur_boundary, direction[i]);
        }
        accumulate_output += cur_boundary;
    }
    for (auto p : pools) {
        delete p;
    }
}


