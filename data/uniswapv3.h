#pragma once
#include "data/pool_base.h"
const int TICK_BASE = 1.0001;
class UniswapV3Pool : public PoolBase {
public:
    const static std::string log_topic;
    static int get_pools(ClientBase* client, std::vector<UniswapV3Pool*>& pools);
    static int get_data(ClientBase* client, uint64_t block_num, std::vector<UniswapV3Pool*>& pools);
    static void add_topics(std::vector<std::string>& topics);
    // 每次新block来时 在处理完logs之后调用
    static int update_data(ClientBase* client, uint64_t block_num = 0);
    const int fee;
    const int tick_space;
    UniswapV3Pool(uint32_t token1_arg, uint32_t token2_arg, std::string address_arg, uint64_t fee_arg, uint64_t tick_space_arg, int tick_size);
    int on_event(const LogEntry& log) override;
    uint256_t sqrt_price;
    uint32_t tick;
    std::vector<uint128_t> liquidities;
private:
};