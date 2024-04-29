#pragma once
#include <sys/time.h>
#include <errno.h>
#include <string>
#include <cstdint>

#include <assert.h>
#include <exception>
#include <string>
#include <list>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <chrono>

#include <gflags/gflags.h>
#include <bthread/bthread.h>
#include <bthread/butex.h>
#include <bvar/bvar.h>
#include <butil/time.h>
#include <bthread/unstable.h>
#include <butil/containers/flat_map.h>

#include <nlohmann/json.hpp>

//#include <butil/comlog_sink.h>
//#include "mylog.h"
#include "util/safe_map.h"

#include <boost/multiprecision/cpp_int.hpp>
#include "util/singleton.h"
#include "util/lock_guard.h"
using uint256_t = boost::multiprecision::uint256_t;
using uint128_t = boost::multiprecision::uint128_t;
using int256_t = boost::multiprecision::int256_t;
using int128_t = boost::multiprecision::int256_t;

using json = nlohmann::json;

inline void save_to_file(const uint256_t& x, std::ofstream& file) {
    std::string x_str = x.str();
    uint32_t str_size = x_str.size();
    file.write(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    file.write(x_str.c_str(), str_size);
}

inline void load_from_file(uint256_t& x, std::ifstream& file) {
    uint32_t str_size;
    file.read(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    std::vector<char> sqrt_price_chars(str_size);
    file.read(sqrt_price_chars.data(), str_size);
    x = boost::multiprecision::uint256_t(std::string(sqrt_price_chars.begin(), sqrt_price_chars.end()));
}

inline void save_to_file(const uint128_t& x, std::ofstream& file) {
    std::string x_str = x.str();
    uint32_t str_size = x_str.size();
    file.write(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    file.write(x_str.c_str(), str_size);
}

inline void save_to_file(const int128_t& x, std::ofstream& file) {
    std::string x_str = x.str();
    uint32_t str_size = x_str.size();
    file.write(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    file.write(x_str.c_str(), str_size);
}

inline void load_from_file(uint128_t& x, std::ifstream& file) {
    uint32_t str_size;
    file.read(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    std::vector<char> sqrt_price_chars(str_size);
    file.read(sqrt_price_chars.data(), str_size);
    x = boost::multiprecision::uint128_t(std::string(sqrt_price_chars.begin(), sqrt_price_chars.end()));
}

inline void load_from_file(int128_t& x, std::ifstream& file) {
    uint32_t str_size;
    file.read(reinterpret_cast<char*>(&str_size), sizeof(str_size));
    std::vector<char> sqrt_price_chars(str_size);
    file.read(sqrt_price_chars.data(), str_size);
    x = boost::multiprecision::int128_t(std::string(sqrt_price_chars.begin(), sqrt_price_chars.end()));
}

template <typename T, typename Z>
void copy_set(const butil::FlatSet<T, Z>& original, butil::FlatSet<T, Z>& copy) {
    for (const T& value : original) {
        copy.insert(value);
    }
}

template <typename K, typename V, typename Z>
void copy_map(const butil::FlatMap<K, V, Z>& original, butil::FlatMap<K, V, Z>& copy) {
    for (const auto& pair : original) {
        copy.insert(pair);
    }
}