#pragma once
#include "util/common.h"
#include "util/singleton.h"
#include "data/client.h"
#include "util/type.h"
#include <simulate/evmc.hpp>
#include <functional>
#include <evmone/evmone.h>

class Transaction;

namespace evmc {
class Code {
public:
    Code(const std::string& str);
    Code(const uint8_t* ptr, size_t len);
    const uint8_t* data() const {
        return _data.data();
    }

    size_t size() const {
        return _data.size();
    }

    bytes32 hash() const {
        return _hash;
    }
    std::string to_string() const;
private:
    std::vector<uint8_t> _data;
    bytes32 _hash = bytes32(0);
};

class SimulateHost;

class SimulateManager {
public:
    static void* wrap_simulate_tx(void* arg);
    SimulateManager(ClientBase* client, uint64_t time_stamp, uint64_t gas_limit, uint256_t base_fee, uint64_t block_number, uint256_t difficulty);
    ~SimulateManager();
    int get_balance(const address& addr, uint256be& val);
    int get_storage(const address& addr, const bytes32& key, bytes32& value);
    int get_code_size(const address& addr, size_t& size);
    int get_code_hash(const address& addr, bytes32& hash);
    int copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data, size_t buffer_size, size_t& out_size);
    int get_block_hash(int64_t block_number, bytes32& hash);
    int get_code(const address& addr, std::shared_ptr<Code>* code = 0);
    uint64_t gas_comsumption(uint64_t gas_limit, const Result& res);
    int get_nonce(const address& addr, uint64_t& nonce);
    void notice_tx(std::shared_ptr<Transaction> tx);
    void simulate_tx_impl(std::shared_ptr<Transaction> tx, int index = 0);
    void release_ptr();
private:
    evmc_message build_message(std::shared_ptr<Transaction> tx, int64_t execution_gas_limit, std::shared_ptr<Code> p) noexcept;
    butil::FlatMap<address, uint256be, std::hash<address>> _balence_map;
    butil::FlatMap<address, std::shared_ptr<Code>, std::hash<address>>  _code_map;
    butil::FlatMap<address, butil::FlatMap<bytes32, bytes32, std::hash<bytes32>>, std::hash<address>> _storage_map;
    butil::FlatMap<int64_t, bytes32> _block_hash;
    butil::FlatMap<address, uint64_t, std::hash<address>> _nonce_map;
    bthread_mutex_t _balance_mutex;
    bthread_mutex_t _code_mutex;
    bthread_mutex_t _storage_mutex;
    bthread_mutex_t _block_hash_mutex;
    bthread_mutex_t _nonce_map_mutex;
    ClientBase* _client;
    VM* _vm;
    // pending block head info
    uint64_t _block_number;
    uint64_t _request_block_number; //for request, =block_number if simulate, otherwise 0
    int64_t _time_stamp;
    uint64_t _gas_limit;
    uint256be _prev_randao;
    bytes32 _chain_id;
    uint256be _base_fee;
    address _block_coinbase;
    uint256be _blob_base_fee;
    // for simulate check
    std::vector<Result> _results;
    std::vector<std::vector<LogEntry>> _logs;
    // for latency
    // approximate to 2 times latency between the node and client
    // in ms
    bvar::LatencyRecorder _request_latency;
    // request count in a single execution
    std::vector<bthread_t> _bids; // simulating threads
    std::atomic<uint32_t> _thread_cnt;
};

}