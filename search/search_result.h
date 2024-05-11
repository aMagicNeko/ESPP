#pragma once
#include "util/common.h"
#include "data/pool_base.h"
struct SearchResult {
    std::vector<PoolBase*> swap_path; // pools index of the path
    uint256_t in; // nums of input token
    uint256_t eth_out; // nums of output eths
    PoolBase* to_eth_pool;
};
