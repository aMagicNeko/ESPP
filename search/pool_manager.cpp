#include "search/pool_manager.h"
#include "data/request.h"
#include "data/error.h"
#include <filesystem>
#include "simulate/prev_logs.h"
#include "data/secure_websocket.h"
#include <unordered_set>
DECLARE_bool(check_simulate_result);
DECLARE_int32(uniswapv3_half_tick_count);
DECLARE_int32(long_request_failed_limit);
DEFINE_int32(logs_step, 10, "requst_logs max block step");
DECLARE_string(host);
DECLARE_string(port);
DECLARE_string(path);
inline bool PoolCmp::operator()(PoolBase* pool1, PoolBase* pool2) const {
    uint32_t token_index = pool1->token1;
    uint256_t token_in = PoolManager::instance()->eth_to_token(token_index, CMP_UNIT);
    uint32_t token_index1 = pool1->token2;
    uint32_t token_index2 = pool2->token2;
    uint256_t token1_out = pool1->compute_output(token_in, true);
    uint256_t token2_out = pool2->compute_output(token_in, true);
    uint256_t eth_out1 = PoolManager::instance()->token_to_eth(token_index1, token1_out);
    uint256_t eth_out2 = PoolManager::instance()->token_to_eth(token_index2, token2_out);
    if (eth_out1 == eth_out2) [[unlikely]] {
        return pool1 < pool2;
    }
    return eth_out1 > eth_out2;
}

inline bool PoolReverseCmp::operator()(PoolBase* pool1, PoolBase* pool2) const {
    uint32_t token_index = pool1->token2;
    uint256_t token_in = PoolManager::instance()->eth_to_token(token_index, CMP_UNIT);
    uint32_t token_index1 = pool1->token1;
    uint32_t token_index2 = pool2->token1;
    uint256_t token1_out = pool1->compute_output(token_in, false);
    uint256_t token2_out = pool2->compute_output(token_in, false);
    uint256_t eth_out1 = PoolManager::instance()->token_to_eth(token_index1, token1_out);
    uint256_t eth_out2 = PoolManager::instance()->token_to_eth(token_index2, token2_out);
    if (eth_out1 == eth_out2) [[unlikely]] {
        return pool1 < pool2;
    }
    return eth_out1 > eth_out2;
}

inline bool TokenCmp::operator()(uint32_t t1, uint32_t t2) const {
    int pool_cnt1 = PoolManager::instance()->_pools_map[t1].size() + PoolManager::instance()->_pools_reverse_map[t1].size();
    int pool_cnt2 = PoolManager::instance()->_pools_map[t2].size() + PoolManager::instance()->_pools_reverse_map[t2].size();
    if (pool_cnt1 == pool_cnt2) {
        return t1 < t2;
    }
    return pool_cnt1 > pool_cnt2;
    //uint256_t weth1 = PoolManager::instance()->_weth_pool_weth_num[t1];
    //uint256_t weth2 = PoolManager::instance()->_weth_pool_weth_num[t2];
    //if (weth1 == weth2) [[unlikely]] {
    //    return t1 < t2;
    //}
    //return weth1 > weth2;
}

// ensure only one thread is processing and for memory fence
static std::atomic<int> s_update_pools_wrap_status;
const Address WETH_ADDR("0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2");
void PoolManager::gen_weth_info() {
    _weth_index = _tokens_index[WETH_ADDR];
    _weth_pool_token_num.resize(_tokens_index.size(), 0);
    _weth_pool_weth_num.resize(_tokens_index.size(), 0);
    for (auto item : _pools_address_map) {
        PoolBase* pool = item.second;
        if (pool->token1 == _weth_index) {
            _weth_pool_token_num[pool->token2] += pool->get_reserve1();
            _weth_pool_weth_num[pool->token2] += pool->get_reserve0();
        }
        else if (pool->token2 == _weth_index) {
            _weth_pool_token_num[pool->token1] += pool->get_reserve0();
            _weth_pool_weth_num[pool->token1] += pool->get_reserve1();
        }
    }
    // weth for weth
    _weth_pool_weth_num[_weth_index] = MAX_TOKEN_NUM;
    _weth_pool_token_num[_weth_index] = MAX_TOKEN_NUM;
    _pools_map.resize(_tokens_index.size());
    _pools_reverse_map.resize(_tokens_index.size());
    for (auto& item : _pools_address_map) {
        PoolBase* pool = item.second;
        auto p = _pools_map[pool->token1].find(pool->token2);
        if (p == _pools_map[pool->token1].end()) {
            p = _pools_map[pool->token1].emplace(pool->token2, std::set<PoolBase*, PoolCmp>()).first;
        }
        p->second.insert(pool);
        auto pp = _pools_reverse_map[pool->token2].find(pool->token1);
        if (pp == _pools_reverse_map[pool->token2].end()) {
            pp = _pools_reverse_map[pool->token2].emplace(pool->token1, std::set<PoolBase*, PoolReverseCmp>()).first;
        }
        pp->second.insert(pool);
    }
}

void PoolManager::save_to_file() {
    std::ofstream file("pools.dat", std::ios::binary);
    if (!file) {
        LOG(ERROR) << "Failed to open file for writing.";
        return;
    }
    file.write(reinterpret_cast<char*>(&_trace_block), sizeof(_trace_block));
    size_t size_tmp = _tokens_address.size();
    file.write(reinterpret_cast<char*>(&size_tmp), sizeof(size_tmp));
    for (Address& a: _tokens_address) {
        a.save_to_file(file);
    }
    size_tmp = _pools_address_map.size(); 
    file.write(reinterpret_cast<char*>(&size_tmp), sizeof(size_tmp));
    for (auto& item:_pools_address_map) {
        item.second->save_to_file(file);
    }
    file.close();
    LOG(INFO) << "save pools success";
}

void PoolManager::load_from_file() {
    std::ifstream file("pools.dat", std::ios::binary);
    if (!file) {
        LOG(ERROR) << "Failed to open file for reading.";
        return;
    }
    file.read(reinterpret_cast<char*>(&_trace_block), sizeof(_trace_block));
    size_t token_size = 0;
    file.read(reinterpret_cast<char*>(&token_size), sizeof(token_size));
    for (size_t cur = 0; cur < token_size; ++cur) {
        Address a;
        a.load_from_file(file);
        _tokens_address.push_back(a);
    }
    size_t pool_size = 0;
    file.read(reinterpret_cast<char*>(&pool_size), sizeof(pool_size));
    for (size_t cur = 0; cur < pool_size; ++cur) {
        auto pool = PoolBase::load_from_file(file);
        if (pool) {
            _pools_address_map.insert(pool->address, pool);
        }
        else {
            LOG(ERROR) << "load pool failed";
            return;
        }
        //LOG(INFO) << cur << "th pool:" << pool->address.to_string() << " token1:" << pool->token1 << " token2:" << pool->token2;
    }
    // _tokens_index
    for (uint32_t i = 0; i < _tokens_address.size(); ++i) {
       _tokens_index.insert(_tokens_address[i], i);
    }
    LOG(INFO) << "load pools from file success";
}

int PoolManager::init(ClientBase* client) {
    _client = new SecureWebsocket;
    _tx_client = client;
    if (_client->connect(FLAGS_host, FLAGS_port, FLAGS_path) != 0) {
        return -1;
    }
    _client->start_listen();
    usleep(1000);
    _tokens_index.init(1);
    _pools_address_map.init(1);
    if (std::filesystem::exists("pools.dat")) {
        load_from_file();
    } else {
        uint64_t block_number = 0;
        if (request_block_number(_client, block_number) != 0) {
            LOG(ERROR) << "get block number failed";
            return -1;
        }
        // init uniswapV2
        std::vector<Address> pools;
        while (UniswapV2Pool::get_pools(_client, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 pools failed";
            reset_connection();
        }
        while (UniswapV2Pool::get_data(_client, block_number, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 data failed";
            reset_connection();
        }
        // init uniswapV3
        std::vector<UniswapV3Pool*> v3pools;
        while (UniswapV3Pool::get_pools(_client, v3pools) != 0) {
            LOG(ERROR) << "get uniswapv3 pools failed";
            reset_connection();
        }
        while (UniswapV3Pool::get_data(_client, block_number, v3pools) != 0) {
            LOG(ERROR) << "get uniswapv3 data failed";
            reset_connection();
        }
        _trace_block = block_number;
        save_to_file();
    }
    // weth info & pools map
    gen_weth_info();
    // get logs topic
    UniswapV2Pool::add_topics(_update_topics);
    UniswapV3Pool::add_topics(_update_topics);
    // update data
    while (update_pools() != 0) {
        LOG(ERROR) << "update pools failed";
        reset_connection();
    }
    save_to_file();
    // for memory fence
    s_update_pools_wrap_status.store(0, std::memory_order_release);
    int ntoken = 0;
    std::set<uint32_t, TokenCmp> token_set;
    for (int i = 0; i < _tokens_address.size(); ++i) {
        if (_weth_pool_token_num[i] != 0) {
            ++ntoken;
        }
        token_set.insert(i);
    }
    //LOG(INFO) << "ntokens directly connected with weth:" << ntoken;
    //for (uint32_t x:token_set) {
    //    LOG(INFO) << _tokens_address[x].to_string() << " num:" << _weth_pool_weth_num[x];
    //}
    //for (auto& [x, y] : _pools_map[_weth_index]) {
    //    LOG(INFO) << "pool order new set";
    //    for (auto pool : y)
    //        LOG(INFO) << "pool order:" << pool->get_liquidit();
    //}
    return 0;
}

void PoolManager::add_token(const Address& token_addr) {
    if (_tokens_index.seek(token_addr) == NULL) {
        _tokens_address.push_back(token_addr);
        _tokens_index.insert(token_addr, _tokens_address.size() - 1);
    }
}

void PoolManager::add_uniswapv2_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint256_t reserve0, uint256_t reserve1) {
    if (_pools_address_map.seek(pool_addr) != 0) {
        return;
    }
    add_token(token0);
    add_token(token1);
    uint32_t index0 = _tokens_index[token0];
    uint32_t index1 = _tokens_index[token1];
    auto pool = new UniswapV2Pool(index0, index1, pool_addr, reserve0 ,reserve1);
    _pools_address_map.insert(pool_addr, pool);
}

UniswapV3Pool* PoolManager::add_uniswapv3_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint64_t fee, uint64_t tickspace) {
    if (_pools_address_map.seek(pool_addr) != 0) {
        return 0;
    }
    add_token(token0);
    add_token(token1);
    uint32_t index0 = _tokens_index[token0];
    uint32_t index1 = _tokens_index[token1];
    auto pool = new UniswapV3Pool(index0, index1, pool_addr, fee, tickspace);
    _pools_address_map.insert(pool_addr, pool);
    return pool;
}

int PoolManager::update_pools() {
    int failed_cnt = 0;
    std::unordered_set<PoolBase*> update_pools;
    while (failed_cnt < FLAGS_long_request_failed_limit) {
        uint64_t cur_block = 0;
        if (request_block_number(_client, cur_block) != 0) [[unlikely]] {
            LOG(ERROR) << "get block number failed";
            ++failed_cnt;
            continue;
        }
        if (_trace_block == cur_block) {
            if (FLAGS_check_simulate_result) [[unlikely]] {
                PrevLogs::instance()->on_head(_client);
            }
            LOG(INFO) << "prepare to update maps";
            _rw_lock.lock_write();
            LOG(INFO) << "update maps ing";
                for (auto pool:update_pools) {
                    _pools_map[pool->token1][pool->token2].erase(pool);
                    _pools_map[pool->token1][pool->token2].insert(pool);
                }
                for (auto pool:update_pools) {
                    _pools_reverse_map[pool->token2][pool->token1].erase(pool);
                    _pools_reverse_map[pool->token2][pool->token1].insert(pool);
                }
            LOG(INFO) << "update maps end";
            _rw_lock.unlock_write();
            return 0;
        }
        uint32_t next = _trace_block + FLAGS_logs_step;
        if (next > cur_block) { 
            next = cur_block;
        }
        std::vector<LogEntry> logs;
        if (request_filter_logs(_client, _trace_block + 1, next, _update_topics, logs) != 0) [[unlikely]] {
            LOG(ERROR) << "request_filter_logs failed";
            ++failed_cnt;
            continue;
        }
        butil::FlatMap<uint32_t, int256_t> weth_pool_token_num_delta;
        butil::FlatMap<uint32_t, int256_t> weth_pool_weth_num_delta;
        weth_pool_token_num_delta.init(1);
        weth_pool_weth_num_delta.init(1);
        for (auto& log : logs) {
            auto p = _pools_address_map.seek(log.address);
            if (p) {
                auto pool = *p;
                update_pools.insert(pool);
                int256_t reserve0 = pool->get_reserve0();
                int256_t reserve1 = pool->get_reserve1();
                if ((*p)->on_event(log) != 0) {
                    LOG(ERROR) << "on_event failed";
                    return -1;
                }
                int256_t reserve0_delta = int256_t(pool->get_reserve0()) - reserve0;
                int256_t reserve1_delta = int256_t(pool->get_reserve1()) - reserve1;
                if (pool->token1 == _weth_index) {
                    weth_pool_token_num_delta[pool->token2] += reserve1_delta;
                    weth_pool_weth_num_delta[pool->token2] += reserve0_delta;
                }
                else if (pool->token2 == _weth_index) {
                    weth_pool_token_num_delta[pool->token1] += reserve0_delta;
                    weth_pool_weth_num_delta[pool->token1] += reserve1_delta;
                }
            }
        }
        if (UniswapV3Pool::update_data(_client, _trace_block) != 0) {
            LOG(ERROR) << "update UniswapV3Pool data failed";
            return -1;
        }
        // update weth info
        for (auto item: weth_pool_token_num_delta) {
            _weth_pool_token_num[item.first] = (int256_t(_weth_pool_token_num[item.first]) + item.second).convert_to<uint256_t>();
        }
        for (auto item: weth_pool_weth_num_delta) {
            _weth_pool_weth_num[item.first] = (int256_t(_weth_pool_weth_num[item.first]) + item.second).convert_to<uint256_t>();
        }
        _trace_block = next;
    }
    return -1;
}

int PoolManager::begin_search(uint64_t number) {
    _rw_lock.lock_read();
    if (_tx_client->number() != number) {
        // old search
        _rw_lock.unlock_read();
        return -1;
    }
    //LOG(INFO) << "search begin";
    return 0;
}

int PoolManager::halt_search(uint64_t number) {
    if (_tx_client->number() != number) {
        // old search
        return -1;
    }
    return 0;
}

void PoolManager::end_search() {
    _rw_lock.unlock_read();
    //LOG(INFO) << "search end";
}

int PoolManager::check_parent(const std::string& parent_hash) const {
    int failed_cnt = 0;
    std::string hash;
    while (failed_cnt <= FLAGS_long_request_failed_limit) {
        if (request_header_hash(_client, _trace_block, hash) == 0) {
            break;
        }
        ++failed_cnt;
    }
    if (parent_hash != hash) {
        LOG(ERROR) << "block hash not match";
        return -1;
    }
    return 0;
}

void* update_pools_wrap(void* arg) {
    auto p = (std::string*)arg;
    // also for memory fence
    if (s_update_pools_wrap_status.fetch_add(1, std::memory_order_acquire) != 0) {
        LOG(INFO) << "another update_pools_wrap is processing";
        s_update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        delete p;
        return NULL;
    }
    //if (PoolManager::instance()->check_parent(*p) != 0) {
    //    LOG(WARNING) << "block reorg!";
    //}
    while (PoolManager::instance()->update_pools() != 0) {
        LOG(INFO) << "update pools failed";
        if (PoolManager::instance()->reset_connection() == 0) {
            continue;
        }
        delete p;
        // for memory fence
        s_update_pools_wrap_status.fetch_sub(1, std::memory_order_release);
        return NULL;
    }
    ErrorHandle::instance()->pools_updated();
    delete p;
    // for memory fence
    s_update_pools_wrap_status.fetch_sub(1, std::memory_order_release);
    return NULL;
}

void PoolManager::on_head(const std::string& parent_hash) {
    bthread_t bid = 0;
    auto p = new std::string(parent_hash);
    bthread_start_background(&bid, nullptr, update_pools_wrap, p);
}

uint256_t PoolManager::token_to_eth(uint32_t token_index, uint256_t input, PoolBase** pool_ret) {
    if (token_index == _weth_index) {
        return input;
    }
    uint256_t max_ret = 0;
    for (auto pool : _pools_map[_weth_index][token_index]) {
        uint256_t out = pool->compute_output(input, false);
        if (out > max_ret) {
            max_ret = out;
            *pool_ret = pool;
        }
    }
    for (auto pool: _pools_reverse_map[token_index][_weth_index]) {
        uint256_t out = pool->compute_output(input, true);
        if (out > max_ret) {
            max_ret = out;
            *pool_ret = pool;
        }
    }
    if (max_ret) {
        LOG(DEBUG) << "eth pool:" << (*pool_ret)->to_string() << " input token:" << input << " eth out:" << max_ret; 
    }
    return max_ret;
}

int PoolManager::reset_connection() {
    _client->stop();
    _client = new SecureWebsocket;
    if (_client->connect(FLAGS_host, FLAGS_port, FLAGS_path) != 0) {
        return -1;
    }
    _client->start_listen();
    usleep(1000);
    return 0;
}

uint256_t PoolManager::token_to_eth(uint32_t token_index, uint256_t token_in) {
    auto nweth = _weth_pool_weth_num[token_index];
    if (nweth == 0) {
        return 0;
    }
    auto ntoken = _weth_pool_token_num[token_index];
    if (ntoken == 0) {
        return MAX_TOKEN_NUM;
    }
    return (uint512_t(token_in) * nweth / ntoken).convert_to<uint256_t>();
}

uint256_t PoolManager::eth_to_token(uint32_t token_index, uint256_t eth_in) {
    auto nweth = _weth_pool_weth_num[token_index];
    if (nweth == 0) {
        return MAX_TOKEN_NUM;
    }
    auto ntoken = _weth_pool_token_num[token_index];
    return (uint512_t(eth_in) * ntoken / nweth).convert_to<uint256_t>();
}

ClientBase* PoolManager::get_client() {
    return static_cast<ClientBase*>(_client);
}