#include "search/online.h"
#include "search/pool_manager.h"
#include "gateway/gateway.h"
#include "simulate/prev_logs.h"
#include "search/analytical_solution.h"
#include "util/latency.h"
DEFINE_bool(check_simulate_result, false, "whether to check logs of simulate");
DEFINE_int32(max_compute_cnt, 500, "max num of ompute paths");
const static uint256_t MIN_LIQUIDITY = 10000000000000;
//static std::atomic<int> only_one_search(0);
std::atomic<int> no_path_cnt(0);
std::atomic<int> find_path_cnt(0);
void OnlineSearch::search(std::shared_ptr<Transaction> tx, const std::vector<LogEntry>& logs, uint64_t number) {
    _block_number = number;
    if (PoolManager::instance()->begin_search(number) != 0) {
        return;
    }
    _tx = tx;
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
    //if (only_one_search.fetch_add(1) != 0) {
    //    only_one_search.fetch_sub(1);
    //    PoolManager::instance()->end_search();
    //    return;
    //}
    if (_pools_address_map.size() == 0) {
        PoolManager::instance()->end_search();
    //    only_one_search.fetch_sub(1);
        return ;
    }
    //LOG(INFO) << "change pools size:" << _pool_direction.size();
    for (auto item : _pool_direction) {
        auto pool = item.first;
        _start_token = item.second ? pool->token1 : pool->token2;
        _path.push_back(pool);
        _direction.push_back(item.second);
        _visited_set.insert(pool->address);
        // first pool has been pushed
        dfs(item.second ? pool->token2 : pool->token1, 2);
        _path.pop_back();
        _direction.pop_back();
        _visited_set.erase(pool->address);
    }
    if (_compute_path_cnt) {
        find_path_cnt.fetch_add(1, std::memory_order_relaxed);
    }
    else {
        no_path_cnt.fetch_add(1, std::memory_order_relaxed);
    }
    //LOG(INFO) << "search complete paths num:" << _compute_path_cnt << " find ratio:" << find_path_cnt.load(std::memory_order_relaxed) / no_path_cnt.load(std::memory_order_relaxed);
    sandwich();
    //only_one_search.fetch_sub(1);
    PoolManager::instance()->end_search();
}

void OnlineSearch::offline_search() {
    uint32_t weth = PoolManager::instance()->weth_index();
    _start_token = weth;
    while (true) {
        dfs(weth, 0);
        
    }
}

inline void OnlineSearch::dfs_impl(PoolBase* pool, uint32_t cur_token, uint32_t len, bool direction) {
    if (_visited_set.seek(pool->address) || (_tx && PoolManager::instance()->halt_search(_block_number))) {
        return;
    }
    _visited_set.insert(pool->address);
    _path.push_back(pool);
    _direction.push_back(direction);
    //LOG(INFO) << "dfsing";
    if (cur_token == _start_token && len) {
        compute();
    }
    else {
        dfs(cur_token, len + 1);
    }
    _direction.pop_back();
    _path.pop_back();
    _visited_set.erase(pool->address);
}

//constexpr int DFS_WIDTH_LIMIT[5] = {0, 0, 200, 50, 0};
constexpr int MAX_LENGTH_LIMIT = 4;
// TODO: random search
void OnlineSearch::dfs(uint32_t cur_token, uint32_t len) {
    if (len > MAX_LENGTH_LIMIT || (_tx && (_compute_path_cnt > FLAGS_max_compute_cnt || PoolManager::instance()->halt_search(_block_number)))) {
        return;
    }
    // LOG(INFO) << "dfs ing:" << len << " cnt:" << _dfs_cnt;
    ++_dfs_cnt;
    auto item = _pools_map.seek(cur_token);
    if (item) {
        for (auto pool:*item) {
            dfs_impl(pool, pool->token2, len, 1);
        }
    }
    auto item1 = _pools_reverse_map.seek(cur_token);
    if (item1) {
        for (auto pool:*item1) {
            dfs_impl(pool, pool->token1, len, 0);
        }
    }
    // turn back to start token
    int cnt = 0;
    auto p = PoolManager::instance()->_pools_map[cur_token].find(_start_token);
    if (p != PoolManager::instance()->_pools_map[cur_token].end()) {
        cnt += p->second.size();
        for (auto pool : p->second) {
            if (pool->get_liquidit() == 0) {
                break;
            }
            assert(pool->token2 == _start_token);
            dfs_impl(pool, pool->token2, len, 1);
        }
    }
    auto pp = PoolManager::instance()->_pools_reverse_map[cur_token].find(_start_token);
    if (pp != PoolManager::instance()->_pools_reverse_map[cur_token].end()) {
        cnt += pp->second.size();
        for (auto pool : pp->second) {
            if (pool->get_liquidit() == 0) {
                break;
            }
            assert(pool->token1 == _start_token);
            dfs_impl(pool, pool->token1, len, 0);
        }
    }
    //LOG(INFO) << "onlinesearch len:" << len << " back_size:" << cnt;
    if (len >= MAX_LENGTH_LIMIT) {
        return;
    }
    for (auto &[t,  pool_set]: PoolManager::instance()->_pools_map[cur_token]) {
        for (auto pool: pool_set) {
            if (pool->token2 == _start_token) {
                // redundant
                continue;
            }
            if (pool->get_liquidit() == 0) {
                break;
            }
            dfs_impl(pool, pool->token2, len, 1);
            if (_compute_path_cnt > FLAGS_max_compute_cnt) {
                return;
            }
        }
        if (_compute_path_cnt > FLAGS_max_compute_cnt) {
            return;
        }
    }
    for (auto &[t, pool_set] : PoolManager::instance()->_pools_reverse_map[cur_token]) {
        for (auto pool: pool_set) {
            if (pool->token1 == _start_token) {
                // redundant
                continue;
            }
            if (pool->get_liquidit() == 0) {
                break;
            }
            dfs_impl(pool, pool->token1, len, 0);
            if (_compute_path_cnt > FLAGS_max_compute_cnt) {
                return;
            }
        }
        if (_compute_path_cnt > FLAGS_max_compute_cnt) {
            return;
        }
    }
}

constexpr int MAX_NROUNDS = 100;
// TODO: add usdt or usdc as endpoint
SearchResult compute_impl(std::vector<std::shared_ptr<PoolBase>> path, std::vector<bool> direction) {
    uint start_index = 0;
    bool direct_connect_to_weth = false;
    for (uint i = 0; i < path.size(); ++i) {
        int cur_token = direction[i] ? path[i]->token1 : path[i]->token2;
        // weth as endpoint
        if (cur_token == PoolManager::instance()->weth_index()) {
            direct_connect_to_weth = true;
            start_index = i;
            break;
        }
        if (direct_connect_to_weth == false && PoolManager::instance()->connect_to_weth(cur_token)) {
            start_index = i;
            direct_connect_to_weth = true;
        }
    }
    if (start_index == 0 && !direct_connect_to_weth) {
        // not connect to weth directly
        return {};
    }
    if (start_index != 0) {
        std::vector<std::shared_ptr<PoolBase>> new_path;
        std::vector<bool> new_direction;
        //LOG(INFO) << "weth as start point";
        for (int i = start_index; i < path.size(); ++i) {
            new_path.push_back(path[i]);
            new_direction.push_back(direction[i]);
        }
        for (int i = 0; i < start_index; ++i) {
            new_path.push_back(path[i]);
            new_direction.push_back(direction[i]);
        }
        path.swap(new_path);
        direction.swap(new_direction);
    }
    uint256_t accumulate_input = 0;
    uint256_t accumulate_output = 0;
    uint256_t max = 0;
    uint256_t res_in = 0;
    //LOG(INFO) << "-------------compute start";
    //for (uint32_t i = 0; i < path.size(); ++i) {
    //    LOG(INFO) << path[i]->to_string();
    //}
    int nrounds = 0;
    while (nrounds++ < MAX_NROUNDS) {
        //LOG(INFO) << "------round start";
        //for (auto p:path) {
            //LOG(INFO) << p->to_string();
        //}
        uint256_t token_in = get_analytical_solution(path, direction);
        //LOG(INFO) << "analytical_solution out:" << token_in;
        if (token_in == 0) {
            break;
        }
        uint256_t cur_boundary = MAX_TOKEN_NUM - 1;
        //LOG(INFO) << "cur boundary:" << cur_boundary;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->get_output_boundary(cur_boundary, direction[i]);
            if (cur_boundary == 0) {
                // end
                break;
            }
            //LOG(INFO) << "cur boundary:" << cur_boundary;
        }
        if (cur_boundary == 0) {
            // end
            break;
        }
        for (int i = path.size() - 1; i >= 0; --i) {
            cur_boundary = path[i]->compute_input(cur_boundary, direction[i]);
            //LOG(INFO) << "cur boundary:" << cur_boundary;
            if (cur_boundary >= MAX_TOKEN_NUM) {
                break;
            }
        }
        if (cur_boundary >= MAX_TOKEN_NUM || cur_boundary == 0) {
            break;
        }
        if (token_in > cur_boundary) {
            token_in = cur_boundary;
        }
        uint256_t token_out = token_in;
        for (uint32_t i = 0; i < path.size(); ++i) {
            token_out = path[i]->compute_output(token_out, direction[i]);
            //LOG(INFO) << "token_out:" << token_out;
        }
        //LOG(DEBUG) << "token in:" << token_in + accumulate_input << " token_out:" << token_out + accumulate_output;
        uint256_t tmp1 = token_out + accumulate_output;
        uint256_t tmp2 = token_in + accumulate_input;
        uint256_t tmp = 0;
        if (tmp1 > tmp2) {
            tmp = tmp1 - tmp2;
        }
        if (tmp > max) {
            max = tmp;
            res_in = token_in + accumulate_input;
        }
        // to next round
        accumulate_input += cur_boundary;
        for (uint32_t i = 0; i < path.size(); ++i) {
            cur_boundary = path[i]->process_swap(cur_boundary, direction[i]);
        }
        if (cur_boundary == 0) {
            // TODO: for liquidity != 0, go on to compute... but need more ticks info
            break;
        }
        accumulate_output += cur_boundary;
        //LOG(INFO) << "boundary out: " << accumulate_output;
    }
    uint32_t token_index = direction[0] ? path[0]->token1 : path[0]->token2;
    PoolBase* to_eth_pool = 0;
    uint256_t eth_out = PoolManager::instance()->token_to_eth(token_index, max, &to_eth_pool);
    SearchResult result {path, res_in, eth_out, to_eth_pool, direction};
    //LOG(INFO) << "compute complete with token_out:" << max << " eth_out:" << eth_out << " token_in:" << res_in;
    return result;
}

void OnlineSearch::compute() {
    std::vector<std::shared_ptr<PoolBase>> pools;
    for (auto pool : _path) {
        pools.push_back(std::shared_ptr<PoolBase>(pool->get_copy()));
    }
    SearchResult result = compute_impl(pools, _direction);
    if (result.eth_out != 0) {
        GateWay::instance()->notice_search_online_result(result, _tx);
        if (_tx) {
            ++_compute_path_cnt;
        }
    }
}

void OnlineSearch::sandwich() {
    
}

void* wrap_offline_search(void* arg) {
    OnlineSearch offline_search;
    offline_search.offline_search();
    return NULL;
}

void OnlineSearch::start_offline_search_thread(int nthreads) {
    for (int i = 0; i < nthreads; ++i) {
        bthread_t bid;
        bthread_start_background(&bid, 0, wrap_offline_search, 0);
    }
}