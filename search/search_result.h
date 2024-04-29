#pragma once
#include "util/common.h"
struct SearchResult {
    std::vector<uint32_t> swap_path; // pools index of the path
    uint256_t in; // nums of input token
    uint256_t out; // nums of output token
};
