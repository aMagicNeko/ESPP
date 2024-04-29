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
    virtual ~UniswapV2Pool() {}
    int on_event(const LogEntry& log) override;
    void save_to_file(std::ofstream& file) override;
    std::string to_string() const override;
    PoolBase* get_copy() override;
    int get_tick() const override; // for debug
    uint256_t get_liquidit() const override;
    uint256_t get_reserve0() const override;
    uint256_t get_reserve1() const override;
    uint32_t get_fee_rate() const override; // * 1e6
    // the swap funntion is smooth below the boundary 
    uint256_t get_output_boundary(uint256_t max_in, bool direction) const override;    
    uint256_t compute_output(uint256_t in, bool direction) const override;
    uint256_t compute_input(uint256_t out, bool direction) const override;
    uint256_t process_swap(uint256_t in, bool direction) override;
};