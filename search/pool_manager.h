#pragma once
#include "data/pool_base.h"
#include "data/tx_pool.h"
#include "data/uniswapv2.h"
#include "data/uniswapv3.h"
class SecureWebsocket;
static const uint256_t CMP_UNIT("1000000000000000000");// 1 weth
struct PoolCmp {
    bool operator()(PoolBase* pool1, PoolBase* pool2) const;
};

struct PoolReverseCmp {
    bool operator()(PoolBase* pool1, PoolBase* pool2) const;
};

class PoolManager : public Singleton<PoolManager> {
public:
    PoolManager() {}
    int init();
    UniswapV3Pool* add_uniswapv3_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint64_t fee, uint64_t tickspace);
    void add_uniswapv2_pool(const Address& pool_addr, const Address& token0, const Address& token1, uint256_t reserve0, uint256_t reserve1);
    void add_token(const Address& token_addr);
    int update_pools();
    void save_to_file();
    void load_from_file();
    int check_parent(const std::string& parent_hash) const;
    void on_head(const std::string& parent_hash);
    uint32_t tokens_num() { return _tokens_index.size(); }
    uint256_t token_to_eth(uint32_t token_index, uint256_t input, PoolBase** pool_ret);
    int reset_connection();
private:
    friend class OfflineSearch;
    friend class OnlineSearch;
    friend class PoolCmp;
    friend class PoolReverseCmp;
    uint256_t token_to_eth(uint32_t token_index, uint256_t token_in);
    uint256_t eth_to_token(uint32_t token_index, uint256_t eth_in);
    void gen_weth_info();
    butil::FlatMap<Address, uint32_t, std::hash<Address>> _tokens_index;
    std::vector<Address> _tokens_address;
    std::vector<std::set<PoolBase*, PoolCmp>> _pools_map;
    std::vector<std::set<PoolBase*, PoolReverseCmp>> _pools_reverse_map;
    butil::FlatMap<Address, PoolBase*, std::hash<Address>> _pools_address_map; // for logs updating
    SecureWebsocket* _client;
    uint64_t _trace_block;
    std::vector<Bytes32> _update_topics;
    uint32_t _weth_index;
    std::vector<uint256_t> _weth_pool_token_num; // 1weth-> ? token
    std::vector<uint256_t> _weth_pool_weth_num; // 1weth-> ? token
    std::vector<std::set<PoolBase*, PoolCmp>> _weth_map; // token1:others, token2:weth
    std::vector<std::set<PoolBase*, PoolReverseCmp>> _reverse_weth_map; // token1:weth, token2:others
};