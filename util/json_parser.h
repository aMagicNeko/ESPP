#pragma once
#include "util/common.h"
#include <charconv>
using json = nlohmann::json;
#define _UINT64_MAX 18446744073709551615ul
int hex_str_to_uint256(const std::string& hexStr, uint256_t& result);
inline int parse_json(const json& json_data, const std::string& key, uint256_t& value) {
    value = 0;
    json::const_iterator it = json_data.find(key);
    if (it == json_data.end()) {
        return -1;
    }
    std::string hex_str = it.value();
    if (hex_str_to_uint256(hex_str, value) != 0) {
        return -1;
    }
    return 0;
}

inline int parse_json(const json& json_data, const std::string& key, uint64_t& value) {
    value = 0;
    json::const_iterator it = json_data.find(key);
    if (it == json_data.end()) {
        return -1;
    }
    std::string hex_str = it.value();
    auto [ptr, ec] = std::from_chars(hex_str.data()+2, hex_str.data() + hex_str.size(), value, 16);
    if (ec == std::errc::invalid_argument) {
        return -1;
    }
    if (ec == std::errc::result_out_of_range) {
        LOG(WARNING) << "value out of range";
        value = _UINT64_MAX;
    }
    return 0;
}

inline int parse_json_id(const json& json_data, const std::string& key, uint32_t& value) {
    // id is decimal
    value = 0;
    json::const_iterator it = json_data.find(key);
    if (it == json_data.end()) {
        return -1;
    }
    if (it->is_null()) {
        return 1;
    }
    value = it.value();
    return 0;
}

inline int parse_json(const json& json_data, const std::string& key, std::string& value) {
    value.clear();
    json::const_iterator it = json_data.find(key);
    if (it == json_data.end()) {
        return -1;
    }
    if (!it.value().is_null()) {
        value = it.value();
    }
    return 0;
}

inline int parse_json(const json& json_data, const std::string& key, json& value) {
    value.clear();
    json::const_iterator it = json_data.find(key);
    if (it == json_data.end()) {
        return -1;
    }
    value = it.value();
    return 0;
}