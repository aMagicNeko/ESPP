#pragma once
#include "util/common.h"
#include "util/type.h"
struct AccessListEntry {
    Address address;
    std::vector<Bytes32> key;
};
// read only for threads other than client
struct Transaction {
    int64_t nonce;
    uint64_t priority_fee;
    uint256_t value;
    Address from;
    Address to;
    uint64_t gas;
    DBytes input;
    // min fee of the account of the all prev pending txs
    uint64_t account_priority_fee; // possibly changed, other threads shouldn't access
    std::string hash;
    uint64_t time_stamp;
    std::vector<AccessListEntry> access_list;
    std::string get_raw_tx(const std::string& secret_key, uint256_t base_fee);
};