#include "search/pool_manager.h"
#include "data/request.h"
#include "data/error.h"
#include <filesystem>
DECLARE_int32(uniswapv3_half_tick_count);
DECLARE_int32(long_request_failed_limit);
void PoolManager::save_to_file() {
    std::ofstream file("pools.dat", std::ios::binary);
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
}

void PoolManager::load_from_file() {
    std::ifstream file("pools.dat", std::ios::binary);
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
        _pools.push_back(PoolBase::load_from_file(file));
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
    for (uint32_t i = 0; i < _tokens_address.size(); ++i) {
        _pools_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
        _pools_reverse_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
    }
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
}

int PoolManager::init(ClientBase* client) {
    _client = client;
    _tokens_index.init(1);
    _pools_address_map.init(1);
    if (std::filesystem::exists("pools.dat")) {
        load_from_file();
    } else {
        uint64_t block_number = 0;
        if (request_block_number(client, block_number) != 0) {
            LOG(ERROR) << "get block number failed";
            return -1;
        }
        // init uniswapV2
        std::vector<Address> pools;
        if (UniswapV2Pool::get_pools(client, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 pools failed";
            return -1;
        }
        if (UniswapV2Pool::get_data(client, block_number, pools) != 0) {
            LOG(ERROR) << "get uniswapv2 data failed";
            return -1;
        }
        // init uniswapV3
        std::vector<UniswapV3Pool*> v3pools;
        if (UniswapV3Pool::get_pools(client, v3pools) != 0) {
            LOG(ERROR) << "get uniswapv3 pools failed";
            return -1;
        }
        if (UniswapV3Pool::get_data(client, block_number, v3pools) != 0) {
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
    return 0;
}

void PoolManager::add_token(const Address& token_addr) {
    if (_tokens_index.seek(token_addr) == NULL) {
        _tokens_address.push_back(token_addr);
        _tokens_index.insert(token_addr, _tokens_address.size() - 1);
        _pools_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
        _pools_map.back().init(1);
        _pools_reverse_map.push_back(butil::FlatMap<uint32_t, std::vector<uint32_t>>());
        _pools_reverse_map.back().init(1);
    }
}

void PoolManager::add_uniswapv2_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint256_t reserve0, uint256_t reserve1) {
    if (_pools_address_map.seek(pool_addr) != 0) {
        return;
    }
    add_token(token0);
    add_token(token0);
    uint32_t index0 = _tokens_index[token0];
    uint32_t index1 = _tokens_index[token0];
    _pools.push_back(new UniswapV2Pool(index0, index1, pool_addr, reserve0 ,reserve1));
    if (_pools_map[index0].seek(index1) == 0) {
        _pools_map[index0].insert(index1, std::vector<uint32_t>());
    }
    _pools_map[index0][index1].push_back(_pools.size() - 1);
    if (_pools_map[index1].seek(index0) == 0) {
        _pools_map[index1].insert(index0, std::vector<uint32_t>());
    }
    _pools_reverse_map[index1][index0].push_back(_pools.size()-1);
    assert(_pools_address_map.seek(pool_addr) == 0);
    _pools_address_map.insert(pool_addr, _pools.size()-1);
}

UniswapV3Pool* PoolManager::add_uniswapv3_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint64_t fee, uint64_t tickspace) {
    if (_pools_address_map.seek(pool_addr) != 0) {
        return 0;
    }
    add_token(token0);
    add_token(token0);
    uint32_t index0 = _tokens_index[token0];
    uint32_t index1 = _tokens_index[token0];
    _pools.push_back(new UniswapV3Pool(index0, index1, pool_addr, fee, tickspace, 2 * FLAGS_uniswapv3_half_tick_count + 1));
    _pools_map[index0][index1].push_back(_pools.size() - 1);
    _pools_reverse_map[index1][index0].push_back(_pools.size()-1);
    assert(_pools_address_map.seek(pool_addr) == 0);
    _pools_address_map.insert(pool_addr, _pools.size()-1);
    return static_cast<UniswapV3Pool*>(_pools.back());
}

int PoolManager::update_pools() {
    uint32_t failed_cnt = 0;
    while (failed_cnt < FLAGS_long_request_failed_limit) {
        uint64_t cur_block = 0;
        if (request_block_number(_client, cur_block) != 0) [[unlikely]] {
            LOG(ERROR) << "get block number failed";
            ++failed_cnt;
            continue;
        }
        if (_trace_block == cur_block) {
            return 0;
        }
        std::vector<LogEntry> logs;
        if (request_filter_logs(_client, _trace_block + 1, _trace_block + 1, _update_topics, logs) != 0) [[unlikely]] {
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
        ++_trace_block;
        if (UniswapV3Pool::update_data(_client, _trace_block) != 0) {
            LOG(ERROR) << "update UniswapV3Pool data failed";
            _trace_block = 0;
            return -1;
        }
    }
    return -1;
}
