#include "search/online.h"
#include "search/pool_manager.h"
#include "gateway/gateway.h"
#include "simulate/prev_logs.h"
#include "search/analytical_solution.h"
DEFINE_int32(online_search_max_length, 4, "max path length of online search");
DEFINE_bool(check_simulate_result, false, "whether to check logs of simulate");
DEFINE_int32(max_compute_cnt, 500, "max num of ompute paths");
const static uint256_t MIN_LIQUIDITY = 10000000000000;
static std::atomic<int> only_one_search(0);
void OnlineSearch::search(std::shared_ptr<Transaction> tx, const std::vector<LogEntry>& logs) {
    _tx = tx;
    _pool_direction.init(1);
    for (auto& log: logs) {
        if (!_pools_address_map.seek(log.address)) {
            auto p = PoolManager::instance()->_pools_address_map.seek(log.address);
            if (p) {
                PoolBase* pool = (*p)->get_copy();
                int d = 0;
                if ((d = pool->on_event(log, true)) < 0) {
                    // not change reserve
                    delete pool;
                    continue;
                }
                _pool_direction.insert({pool, !d});
                _pools_address_map.insert(log.address, pool);
                uint32_t token1 = pool->token1;
                uint32_t token2 = pool->token2;
                if (!d) {
                    auto pp = _pools_map.seek(token1);
                    if (pp == 0) {
                        pp = _pools_map.insert(token1, std::set<PoolBase*, PoolCmp>());
                    }
                    pp->insert(pool);
                }
                else {
                    auto pp = _pools_reverse_map.seek(token2);
                    if (pp == 0) {
                        pp = _pools_reverse_map.insert(token2, std::set<PoolBase*, PoolReverseCmp>());
                    }
                    pp->insert(pool);
                }
                if (FLAGS_check_simulate_result) [[unlikely]] {
                    PrevLogs::instance()->add_log(tx->hash, log);
                }
            }
        }
        else {
            _pools_address_map[log.address]->on_event(log, true);
        }
    }
    if (_pools_address_map.size()) {
        if (only_one_search.fetch_add(1) != 0)
            return;
    }
    while (_len_limit <= FLAGS_online_search_max_length && _compute_path_cnt <= FLAGS_max_compute_cnt) {
        for (auto item : _pool_direction) {
            auto pool = item.first;
            _start_token = item.second ? pool->token1 : pool->token2;
            _path.push_back(item.first);
            _direction.push_back(item.second);
            _visited_set.insert(pool->address);
            LOG(DEBUG) << "dfs start:" << item.first;
            dfs(_start_token, 1);
            _path.pop_back();
            _direction.pop_back();
            _visited_set.erase(pool->address);
        }
        ++_len_limit;
    }
    LOG(INFO) << "online search complete with paths num:" << _compute_path_cnt;
    sandwich();
}

inline void OnlineSearch::dfs_impl(PoolBase* pool, uint32_t cur_token, uint32_t len, bool direction) {
    if (_visited_set.seek(pool->address)) {
        return;
    }
    _visited_set.insert(pool->address);
    _path.push_back(pool);
    _direction.push_back(direction);
    if (cur_token == _start_token) {
        compute();
    }
    else {
        dfs(cur_token, len + 1);
    }
    _direction.pop_back();
    _path.pop_back();
    _visited_set.erase(pool->address);
}

void OnlineSearch::dfs(uint32_t cur_token, uint32_t len) {
    if (len > _len_limit || _compute_path_cnt > FLAGS_max_compute_cnt) {
        return;
    }
    auto item = _pools_map.seek(cur_token);
    if (item) {
        for (auto pool:*item) {
            dfs_impl(pool, pool->token2, len, 1);
        }
    }
    auto item1 = _pools_reverse_map.seek(cur_token);
    if (item1) {
        for (auto pool:*item) {
            dfs_impl(pool, pool->token2, len, 0);
        }
    }
    for (auto pool : PoolManager::instance()->_pools_map[cur_token]) {
        dfs_impl(pool, pool->token1, len, 1);
    }
    for (auto pool : PoolManager::instance()->_pools_reverse_map[cur_token]) {
        dfs_impl(pool, pool->token2, len, 1);
    }
}
const uint256_t MAX_TOKEN_NUM = (uint256_t(1) << 112);

SearchResult compute_impl(const std::vector<PoolBase*>& path, const std::vector<bool>& direction) {
    uint256_t accumulate_input = 0;
    uint256_t accumulate_output = 0;
    uint256_t max = 0;
    uint256_t res_in = 0;
    LOG(DEBUG) << "-------------compute start";
    for (uint32_t i = 0; i < path.size(); ++i) {
        LOG(DEBUG) << "tick: " << path[i]->get_tick() << " liquidity:" << path[i]->get_liquidit();
    }
    while (true) {
        LOG(DEBUG) << "------round start";
        uint256_t cur_boundary = MAX_TOKEN_NUM - 1;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->get_output_boundary(cur_boundary, direction[i]);
            if (cur_boundary == 0) {
                // end
                break;
            }
        }
        if (cur_boundary == 0) {
            // end
            break;
        }
        for (int i = path.size() - 1; i >= 0; --i) {
            cur_boundary = path[i]->compute_input(cur_boundary, direction[i]);
            if (cur_boundary >= MAX_TOKEN_NUM) {
                break;
            }
        }
        if (cur_boundary >= MAX_TOKEN_NUM) {
            break;
        }
        uint256_t token_in = get_analytical_solution(path, direction, cur_boundary);
        LOG(DEBUG) << "analytical_solution out:" << token_in;
        if (token_in > cur_boundary) {
            token_in = cur_boundary;
        }
        uint256_t token_out = token_in;
        for (uint32_t i = 0; i < path.size(); ++i) {
            token_out = path[i]->compute_output(token_out, direction[i]);
        }
        LOG(DEBUG) << "token in:" << token_in + accumulate_input << " token_out:" << token_out + accumulate_output;
        uint256_t tmp = token_out + accumulate_output - token_in - accumulate_input;
        if (tmp > max) {
            max = tmp;
            res_in = token_in + accumulate_input;
        }
        // to next round
        accumulate_input += cur_boundary;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->process_swap(cur_boundary, direction[i]);
        }
        accumulate_output += cur_boundary;
    }
    uint32_t token_index = direction[0] ? path[0]->token1 : path[0]->token2;
    PoolBase* to_eth_pool = 0;
    uint256_t eth_out = PoolManager::instance()->token_to_eth(token_index, max, &to_eth_pool);
    SearchResult result {path, res_in, eth_out, to_eth_pool};
    for (auto p : path) {
        delete p;
    }
    LOG(INFO) << "compute complete with token_out:" << max << " eth_out:" << eth_out << " token_in:" << res_in;
    return result;
}

void OnlineSearch::compute() {
    std::vector<PoolBase*> pools;
    for (auto pool : _path) {
        pools.push_back(pool->get_copy());
    }
    std::stringstream ss;
    ss << _start_token << ' ';
    for (int i = 0; i < pools.size(); ++i) {
        if (_direction[i]) {
            ss << pools[i]->token2 << ' ';
        }
        else {
            ss << pools[i]->token1 << ' ';
        }
    }
    LOG(DEBUG) << "compute start index:" << _compute_path_cnt << " path:" << ss.str();
    SearchResult result = compute_impl(pools, _direction);
    ++_compute_path_cnt;
    GateWay::instance()->notice_search_online_result(result, _tx);
    for (auto p:pools) {
        delete p;
    }
}

void OnlineSearch::sandwich() {
    
}

