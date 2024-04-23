#pragma once
#include "data/pool_base.h"

class UniswapV2Pool : public PoolBase {
public:
    static int get_pools(ClientBase* client, std::vector<std::string>& pools);
    static int get_data(ClientBase* client, uint64_t block_num, const std::vector<std::string>& pools);
    static void add_topics(std::vector<std::string>& topics);
    UniswapV2Pool(uint32_t token1_arg, uint32_t token2_arg, std::string address_arg, uint256_t reserve0_arg, uint256_t reserve1_arg);
    int on_event(const LogEntry& log) override;
private:
    uint256_t _reserve0;
    uint256_t _reserve1;
};

const int MIN_LIQ = 1000;