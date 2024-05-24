#pragma once
#include "data/pool_base.h"
#include "data/tx_pool.h"
#include "data/uniswapv2.h"
#include "data/uniswapv3.h"
#include "util/rw_lock.h"

class SecureWebsocket;
static const uint256_t CMP_UNIT("1000000000000000000");// 1 weth
struct PoolCmp {
    bool operator()(PoolBase* pool1, PoolBase* pool2) const;
};

struct PoolReverseCmp {
    bool operator()(PoolBase* pool1, PoolBase* pool2) const;
};

struct TokenCmp {
    bool operator()(uint32_t t1, uint32_t t2) const;
};

class PoolManager : public Singleton<PoolManager> {
public:
    PoolManager() {}
    int init(ClientBase* client);
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
    ClientBase* get_client();
    int begin_search(uint64_t number);
    int halt_search(uint64_t number);
    void end_search();
    uint32_t weth_index() const {
        return _weth_index;
    }
    bool connect_to_weth(uint32_t index) const {
        return (_pools_map[_weth_index].count(index) != 0) || (_pools_reverse_map[_weth_index].count(index) != 0);
    }
private:
    friend class OfflineSearch;
    friend class OnlineSearch;
    friend class PoolCmp;
    friend class PoolReverseCmp;
    friend class TokenCmp;
    friend class GateWay;
    uint256_t token_to_eth(uint32_t token_index, uint256_t token_in);
    uint256_t eth_to_token(uint32_t token_index, uint256_t eth_in);
    void gen_weth_info();
    butil::FlatMap<Address, uint32_t, std::hash<Address>> _tokens_index;
    std::vector<Address> _tokens_address;
    std::vector<std::map<uint32_t, std::set<PoolBase*, PoolCmp>, TokenCmp>> _pools_map;
    std::vector<std::map<uint32_t, std::set<PoolBase*, PoolReverseCmp>, TokenCmp>> _pools_reverse_map;
    butil::FlatMap<Address, PoolBase*, std::hash<Address>> _pools_address_map; // for logs updating
    SecureWebsocket* _client;
    ClientBase* _tx_client;
    uint64_t _trace_block;
    std::vector<Bytes32> _update_topics;
    uint32_t _weth_index;
    std::vector<uint256_t> _weth_pool_token_num; // 1weth-> ? token
    std::vector<uint256_t> _weth_pool_weth_num; // 1weth-> ? token
    ReadWriteLock _rw_lock;
};