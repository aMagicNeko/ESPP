#pragma once
#include "data/pool_base.h"
uint256_t get_analytical_solution_length2(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction);
uint256_t get_analytical_solution_length3(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction);
uint256_t get_analytical_solution_length4(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction);
uint256_t get_analytical_solution_length5(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction);
uint256_t get_analytical_solution_length6(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction);

inline uint256_t get_analytical_solution(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    if (pools.size() == 2) {
        return get_analytical_solution_length2(pools, direction);
    }
    else if (pools.size() == 3) {
        return get_analytical_solution_length3(pools, direction);
    }
    else if (pools.size() == 4) {
        return get_analytical_solution_length4(pools, direction);
    }
    else if (pools.size() == 5) {
        return get_analytical_solution_length5(pools, direction);
    }
    else if (pools.size() == 6) {
        return get_analytical_solution_length6(pools, direction);
    }
    else {
        LOG(ERROR) << "not implemented path length:" << pools.size();
        return -1;
    }
}
