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
//#include <brpc/socket.h>
//base::FlatMap and base::FlatSet
#include <butil/containers/flat_map.h>

#include <nlohmann/json.hpp>

//#include <butil/comlog_sink.h>
//#include "mylog.h"
#include "util/safe_map.h"

#include <boost/multiprecision/cpp_int.hpp>
#include "util/solidity_type.h"
#include "util/singleton.h"
#include "util/lock_guard.h"
using uint256_t = boost::multiprecision::uint256_t;
using uint128_t = boost::multiprecision::uint128_t;
using json = nlohmann::json;

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