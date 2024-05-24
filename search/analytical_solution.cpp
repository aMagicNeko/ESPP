#include "search/analytical_solution.h"
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/math/special_functions/sqrt1pm1.hpp>

typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<288>> float256_t;
using boost::multiprecision::sqrt;
inline float256_t get_x(std::shared_ptr<PoolBase> pool, bool direction) {
    return (direction ? pool->get_reserve0() : pool->get_reserve1()).convert_to<float256_t>();
}

inline float256_t get_y(std::shared_ptr<PoolBase> pool, bool direction) {
    return (direction ? pool->get_reserve1() : pool->get_reserve0()).convert_to<float256_t>();
}

inline float256_t get_k(std::shared_ptr<PoolBase> pool, bool direction) {
    return 1 - float256_t(pool->get_fee_rate()) / 1000000;
}

uint256_t get_analytical_solution_length2(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    std::vector<float256_t> x(2);
    std::vector<float256_t> y(2);
    std::vector<float256_t> k(2);
    for (uint32_t i = 0; i < 2; ++i) {
        x[i] = get_x(pools[i], direction[i]);
        y[i] = get_y(pools[i], direction[i]);
        k[i] = get_k(pools[i], direction[i]);
    }
    float256_t numerator = -k[0] * x[0] * x[1] + sqrt(k[0] * k[0] * k[0] * k[1] * x[0] * x[1] * y[0] * y[1]);
    if (numerator < 0) {
        return 0;
    }
    float256_t denominator = k[0] * k[0] * (k[1] * y[0] + x[1]);
    return uint256_t(numerator / denominator);
}

uint256_t get_analytical_solution_length3(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    std::vector<float256_t> x(3);
    std::vector<float256_t> y(3);
    std::vector<float256_t> k(3);
    for (uint32_t i = 0; i < 3; ++i) {
        x[i] = get_x(pools[i], direction[i]);
        y[i] = get_y(pools[i], direction[i]);
        k[i] = get_k(pools[i], direction[i]);
    }
    float256_t numerator = -k[0] * x[0] * x[1] * x[2] + sqrt(k[0] * k[0] * k[0] * k[1] * k[2] * x[0] * x[1] * x[2] * y[0] * y[1] * y[2]);
    if (numerator < 0) {
        return 0;
    }
    float256_t denominator = k[0] * k[0] * (k[1] * k[2] * y[0] * y[1] + k[1] * x[2] * y[0] + x[1] * x[2]);
    return uint256_t(numerator / denominator);
}

uint256_t get_analytical_solution_length4(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    std::vector<float256_t> x(4);
    std::vector<float256_t> y(4);
    std::vector<float256_t> k(4);
    for (uint32_t i = 0; i < 4; ++i) {
        x[i] = get_x(pools[i], direction[i]);
        y[i] = get_y(pools[i], direction[i]);
        k[i] = get_k(pools[i], direction[i]);
    }
    float256_t numerator = -k[0] * x[0] * x[1] * x[2] * x[3] + sqrt(k[0] * k[0] * k[0] * k[1] * k[2] * k[3] * x[0] * x[1] * x[2] * x[3] * y[0] * y[1] * y[2] * y[3]);
    if (numerator < 0) {
        return 0;
    }
    float256_t denominator = k[0] * k[0] * (k[1] * k[2] * k[3] * y[0] * y[1] * y[2] + k[1] * k[2] * x[3] * y[0] * y[1] + k[1] * x[2] * x[3] * y[0] + x[1] * x[2] * x[3]);
    return uint256_t(numerator / denominator);
}

uint256_t get_analytical_solution_length5(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    std::vector<float256_t> x(5);
    std::vector<float256_t> y(5);
    std::vector<float256_t> k(5);
    for (uint32_t i = 0; i < 5; ++i) {
        x[i] = get_x(pools[i], direction[i]);
        y[i] = get_y(pools[i], direction[i]);
        k[i] = get_k(pools[i], direction[i]);
    }
    float256_t numerator = -k[0] * x[0] * x[1] * x[2] * x[3] * x[4] + sqrt(k[0] * k[0] * k[0] * k[1] * k[2] * k[3] * k[4] * x[0] * x[1] * x[2] * x[3] * x[4] * y[0] * y[1] * y[2] * y[3] * y[4]);
    if (numerator < 0) {
        return 0;
    }
    float256_t denominator = k[0] * k[0] * (k[1] * k[2] * k[3] * k[4] * y[0] * y[1] * y[2] * y[3] + k[1] * k[2] * k[3] * x[4] * y[0] * y[1] * y[2] + k[1] * k[2] * x[3] * x[4] * y[0] * y[1] + k[1] * x[2] * x[3] * x[4] * y[0] + x[1] * x[2] * x[3] * x[4]);
    return uint256_t(numerator / denominator);
}

uint256_t get_analytical_solution_length6(const std::vector<std::shared_ptr<PoolBase>>& pools, const std::vector<bool>& direction) {
    std::vector<float256_t> x(6);
    std::vector<float256_t> y(6);
    std::vector<float256_t> k(6);
    for (uint32_t i = 0; i < 6; ++i) {
        x[i] = get_x(pools[i], direction[i]);
        y[i] = get_y(pools[i], direction[i]);
        k[i] = get_k(pools[i], direction[i]);
    }
    float256_t numerator = -k[0] * x[0] * x[1] * x[2] * x[3] * x[4] * x[5] + sqrt(k[0] * k[0] * k[0] * k[1] * k[2] * k[3] * k[4] * k[5] * x[0] * x[1] * x[2] * x[3] * x[4] * x[5] * y[0] * y[1] * y[2] * y[3] * y[4] * y[5]);
    if (numerator < 0) {
        return 0;
    }
    float256_t denominator = k[0] * k[0] * (k[1] * k[2] * k[3] * k[4] * k[5] * y[0] * y[1] * y[2] * y[3] * y[4] + k[1] * k[2] * k[3] * k[4] * x[5] * y[0] * y[1] * y[2] * y[3] + k[1] * k[2] * k[3] * x[4] * x[5] * y[0] * y[1] * y[2] + k[1] * k[2] * x[3] * x[4] * x[5] * y[0] * y[1] + k[1] * x[2] * x[3] * x[4] * x[5] * y[0] + x[1] * x[2] * x[3] * x[4] * x[5]);
    return uint256_t(numerator / denominator);
}