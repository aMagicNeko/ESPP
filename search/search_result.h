#pragma once
#include "util/common.h"
#include "data/pool_base.h"
struct SearchResult {
    std::vector<std::shared_ptr<PoolBase>> swap_path; // pools index of the path
    uint256_t in = 0; // nums of input token
    uint256_t eth_out = 0; // nums of output eths
    PoolBase* to_eth_pool = 0;
    std::vector<bool> direction;
    std::string to_string() const {
        std::stringstream ss;
        ss << "in: " << in << " eth_out:" << eth_out;
        for (auto p:swap_path) {
            ss << p->to_string();
        }
        return ss.str();
    }
};
