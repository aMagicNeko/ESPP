#include "search/pool_manager.h"
#include "data/request.h"
#include "data/error.h"
#include <filesystem>
#include "simulate/prev_logs.h"
#include "data/secure_websocket.h"
DECLARE_bool(check_simulate_result);
DECLARE_int32(uniswapv3_half_tick_count);
DECLARE_int32(long_request_failed_limit);
DEFINE_int32(logs_step, 10, "requst_logs max block step");
DECLARE_string(host);
DECLARE_string(port);
DECLARE_string(path);
// ensure only one thread is processing and for memory fence
static std::atomic<int> s_update_pools_wrap_status;

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
    size_tmp = _pools.size(); 
    file.write(reinterpret_cast<char*>(&size_tmp), sizeof(size_tmp));
    for (PoolBase* pool:_pools) {
        pool->save_to_file(file);
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
            _pools.push_back(pool);
        }
        LOG(INFO) << cur << "th pool:" << pool->address.to_string() << " token1:" << pool->token1 << " token2:" << pool->token2;
    }
    // _tokens_index
    for (uint32_t i = 0; i < _tokens_address.size(); ++i) {
       _tokens_index.insert(_tokens_address[i], i);
    }
    // _pools_address_map
    for (uint32_t i = 0; i < _pools.size(); ++i) {
        _pools_address_map.insert(_pools[i]->address, i);
    }
    // _pools_map
    for (uint32_t i = 0; i < _tokens_address.size() + 1; ++i) {
        _pools_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
        _pools_reverse_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
    }
    for (auto&m : _pools_map) {
        m.init(1);
    }
    for (auto&m : _pools_reverse_map) {
        m.init(1);
    }
    LOG(INFO) << "token size:" << _tokens_address.size();

    for (uint32_t i = 0; i < _pools.size(); ++i) {
        PoolBase* pool = _pools[i];
        if (_pools_map[pool->token1].seek(pool->token2) == 0) {
            _pools_map[pool->token1].insert(pool->token2, std::vector<uint32_t>());
        }
        _pools_map[pool->token1][pool->token2].push_back(i);
        if (_pools_reverse_map[pool->token2].seek(pool->token1) == 0) {
            _pools_reverse_map[pool->token2].insert(pool->token1, std::vector<uint32_t>());
        }
        _pools_reverse_map[pool->token2][pool->token1].push_back(i);
    }
    LOG(INFO) << "load pools from file success";
}
const Address WETH_ADDR("0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2");

int PoolManager::init() {
    _client = new SecureWebsocket;
    if (_client->connect(FLAGS_host, FLAGS_port, FLAGS_path) != 0) {
        return -1;
    }
    _client->start_listen();
    usleep(1000);
    _tokens_index.init(1);
    _pools_address_map.init(1);
    add_token(WETH_ADDR);
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
        if (UniswapV2Pool::get_pools(_client, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 pools failed";
            return -1;
        }
        if (UniswapV2Pool::get_data(_client, block_number, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 data failed";
            return -1;
        }
        // init uniswapV3
        std::vector<UniswapV3Pool*> v3pools;
        if (UniswapV3Pool::get_pools(_client, v3pools) != 0) {
            LOG(ERROR) << "get uniswapv3 pools failed";
            return -1;
        }
        if (UniswapV3Pool::get_data(_client, block_number, v3pools) != 0) {
            LOG(ERROR) << "get uniswapv3 data failed";
            return -1;
        }
        _trace_block = block_number;
    }
    // get logs topic
    UniswapV2Pool::add_topics(_update_topics);
    UniswapV3Pool::add_topics(_update_topics);
    // update data
    if (update_pools() != 0) {
        LOG(ERROR) << "update pools failed";
        return -1;
    }
    save_to_file();
    // for memory fence
    s_update_pools_wrap_status.store(0, std::memory_order_release);
    return 0;
}

void PoolManager::add_token(const Address& token_addr) {
    if (_tokens_index.seek(token_addr) == NULL) {
        _tokens_address.push_back(token_addr);
        _tokens_index.insert(token_addr, _tokens_address.size() - 1);
        _pools_map.resize(_tokens_index.size());
        _pools_reverse_map.resize(_tokens_index.size());
        //_pools_map[_tokens_index.size() - 1].init(1);
        //_pools_reverse_map[_tokens_index.size() - 1].init(1);
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
    _pools.push_back(new UniswapV2Pool(index0, index1, pool_addr, reserve0 ,reserve1));
    _pools_map[index0].init(1);
    if (_pools_map[index0].seek(index1) == 0) {
        _pools_map[index0].insert(index1, std::vector<uint32_t>());
    }
    _pools_map[index0][index1].push_back(_pools.size() - 1);
    _pools_reverse_map[index1].init(1);
    if (_pools_reverse_map[index1].seek(index0) == 0) {
        _pools_reverse_map[index1].insert(index0, std::vector<uint32_t>());
    }
    _pools_reverse_map[index1][index0].push_back(_pools.size()-1);
    _pools_address_map.insert(pool_addr, _pools.size()-1);
}

UniswapV3Pool* PoolManager::add_uniswapv3_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint64_t fee, uint64_t tickspace) {
    if (_pools_address_map.seek(pool_addr) != 0) {
        return 0;
    }
    add_token(token0);
    add_token(token1);
    uint32_t index0 = _tokens_index[token0];
    uint32_t index1 = _tokens_index[token1];
    auto p = new UniswapV3Pool(index0, index1, pool_addr, fee, tickspace);
    _pools.push_back(p);
    _pools_map[index0].init(1);
    if (_pools_map[index0].seek(index1) == 0) {
        _pools_map[index0].insert(index1, std::vector<uint32_t>());
    }
    _pools_map[index0][index1].push_back(_pools.size() - 1);
    _pools_reverse_map[index1].init(1);
    if (_pools_reverse_map[index1].seek(index0) == 0) {
        _pools_reverse_map[index1].insert(index0, std::vector<uint32_t>());
    }
    _pools_reverse_map[index1][index0].push_back(_pools.size()-1);
    _pools_address_map.insert(pool_addr, _pools.size()-1);
    return p;
}

int PoolManager::update_pools() {
    int failed_cnt = 0;
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
        for (auto& log : logs) {
            auto p = _pools_address_map.seek(log.address);
            if (p) {
                if (_pools[*p]->on_event(log) != 0) {
                    LOG(ERROR) << "on_event failed";
                    _trace_block = 0;
                    return -1;
                }
            }
        }
        _trace_block = next;
        if (UniswapV3Pool::update_data(_client, _trace_block) != 0) {
            LOG(ERROR) << "update UniswapV3Pool data failed";
            _trace_block = 0;
            return -1;
        }
    }
    return -1;
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
    if (PoolManager::instance()->check_parent(*p) != 0) {
        LOG(WARNING) << "block reorg!";
    }
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

uint256_t PoolManager::token_to_eth(uint32_t token_index, uint256_t input, uint32_t& pool_index) {
    uint256_t max_ret = 0;
    for (auto p : _pools_map[0][token_index]) {
        PoolBase* pool = _pools[p]->get_copy();
        uint256_t out = pool->compute_output(input, 1);
        if (out > max_ret) {
            max_ret = out;
            pool_index = p;
        }
    }
    for (auto p : _pools_map[token_index][0]) {
        PoolBase* pool = _pools[p]->get_copy();
        uint256_t out = pool->compute_output(input, 0);
        if (out > max_ret) {
            max_ret = out;
            pool_index = p;
        }
    }
    return max_ret;
}

int PoolManager::reset_connection() {
    _client->stop();
    delete _client;
    _client = new SecureWebsocket;
    if (_client->connect(FLAGS_host, FLAGS_port, FLAGS_path) != 0) {
        return -1;
    }
    _client->start_listen();
    usleep(1000);
    return 0;
}