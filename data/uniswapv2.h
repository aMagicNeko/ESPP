#pragma once
#include "data/abi.h"

class UniswapV2Abi : public AbiBase {
public:
    int init(ClientBase* client) override;
    int get_data(ClientBase* client, uint64_t block_num, const std::vector<std::string>& pools) override;
    int on_event(uint64_t pool_index, const json& json_data) override;
    std::string get_logs_head() override;
private:
    std::vector<uint256_t> _reserve0;
    std::vector<uint256_t> _reserve1;
    std::string _mint_head;
    std::string _burn_head;
    std::string _swap_head;
    std::string _sync_head;
    std::string _transfer_head;
};
