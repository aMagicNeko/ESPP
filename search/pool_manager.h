#pragma once
#include "data/pool_base.h"
#include "data/tx_pool.h"
#include "data/uniswapv2.h"
#include "data/uniswapv3.h"
class PoolManager : public Singleton<PoolManager> {
public:
    PoolManager() {}
    int init(ClientBase* client);
    UniswapV3Pool* add_uniswapv3_pool(std::string pool_addr, std::string token0, std::string token1, uint64_t fee, uint64_t tickspace);
    void add_uniswapv2_pool(std::string pool_addr, std::string token0, std::string token1, uint256_t reserve0, uint256_t reserve1);
    void add_token(std::string token_addr);
    int update_pools();
private:
    butil::FlatMap<std::string, uint32_t> _tokens_index;
    std::vector<std::string> _tokens_address;
    std::vector<butil::FlatMap<uint32_t, std::vector<int>>> _pools_map;
    std::vector<butil::FlatMap<uint32_t, std::vector<int>>> _pools_reverse_map;
    std::vector<PoolBase*> _pools;
    butil::FlatMap<std::string, uint32_t> _pools_address_map; // for logs updating
    ClientBase* _client;
    uint64_t _trace_block;
    std::vector<std::string> _update_topics;
};