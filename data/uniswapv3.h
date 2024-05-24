#pragma once
#include "data/pool_base.h"
const int TICK_BASE = 1.0001;
class UniswapV3Pool : public PoolBase {
public:
    const static std::string log_topic;
    static int get_pools(ClientBase* client, std::vector<UniswapV3Pool*>& pools);
    static int get_data(ClientBase* client, uint64_t block_num, std::vector<UniswapV3Pool*>& pools);
    static void add_topics(std::vector<Bytes32>& topics);
    // on_head, after calling all on_event
    static int update_data(ClientBase* client, uint64_t block_num = 0);
    int fee;
    int tick_space;
    uint256_t sqrt_price; // x96
    int32_t tick;
    uint128_t liquidity;
    std::map<int, int128_t> liquidity_net; // we only save ticks within a word or in neighbor word
    UniswapV3Pool(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg, uint64_t fee_arg, uint64_t tick_space_arg);
    virtual ~UniswapV3Pool() {}
    int on_event(const LogEntry& log, bool pending = 0) override;
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
    // due to tick info not complete, might left some in after moving out from all ticks, in this case return 0
    uint256_t compute_output(uint256_t in, bool direction) const override;
    uint256_t compute_input(uint256_t out, bool direction) const override;
    uint256_t process_swap(uint256_t in, bool direction) override;
    PoolType type() override {
        return UniswapV3;
    }
private:
    uint256_t compute_output_impl(uint256_t in, bool direction, int32_t& tick_after, uint256_t& ratio_after, uint128_t& liquidity_after) const;
};