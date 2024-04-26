#pragma once
#include "data/pool_base.h"

class UniswapV2Pool : public PoolBase {
public:
    static int get_pools(ClientBase* client, std::vector<Address>& pools);
    static int get_data(ClientBase* client, uint64_t block_num, const std::vector<Address>& pools);
    static void add_topics(std::vector<Bytes32>& topics);
    uint128_t _reserve0; // uint112
    uint128_t _reserve1; // uint112
    UniswapV2Pool(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg, uint256_t reserve0_arg, uint256_t reserve1_arg);
    int on_event(const LogEntry& log) override;
    void save_to_file(std::ofstream& file) override;
    void get_input_intervals(std::vector<std::pair<uint128_t, uint128_t>>& i) override;
};

const int MIN_LIQ = 1000;