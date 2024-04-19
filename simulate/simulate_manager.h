#pragma once
#include "util/common.h"
#include "util/singleton.h"
#include "data/client.h"
#include <simulate/evmc.hpp>
#include <functional>
#include <evmone/evmone.h>

class Transaction;

namespace evmc {
class Code {
public:
    //Code() {}
    Code(const std::string& str);
    Code(const uint8_t* ptr, size_t len);
    //~Code() = default;
    //Code(const Code& code) = default;
    //Code& operator=(const Code& code) = default;
    //Code(Code&& code) noexcept = default;
    //Code& operator=(Code&& code) noexcept = default;

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

struct LogEntry {
    address addr;               // 日志来源地址
    std::vector<uint8_t> data;  // 日志数据
    std::vector<bytes32> topics;// 日志主题

    // 构造函数，用于初始化日志条目
    LogEntry(const address& addr, const uint8_t* data, size_t data_size, const bytes32 topics[], size_t num_topics)
    : addr(addr), data(data, data + data_size), topics(topics, topics + num_topics) {
        // 这里使用了vector的范围构造函数来初始化data和topics
    }
    /// Equal operator.
    bool operator==(const LogEntry& other) const noexcept
    {
        return addr == other.addr && data == other.data && topics == other.topics;
    }

    std::string to_string() const;
};

class SimulateManager : public Singleton<SimulateManager> {
public:
    SimulateManager();
    void start(ClientBase* client);
    int get_balance(const address& addr, uint256be& val);
    int get_storage(const address& addr, const bytes32& key, bytes32& value);
    int get_code_size(const address& addr, size_t& size);
    int get_code_hash(const address& addr, bytes32& hash);
    int copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data, size_t buffer_size, size_t& out_size);
    int get_block_hash(int64_t block_number, bytes32& hash);
    int get_code(const address& addr, std::shared_ptr<Code>* code = 0);
    void notice_change(size_t change_idx); // 由tx_pool调用 通知pending_txs变化
    void run_in_loop();
    void set_head_info(uint64_t time_stamp, uint64_t gas_limit, uint256_t base_fee, uint64_t block_number, uint256_t difficulty); // 新 head 来时由client调用
    uint64_t gas_comsumption(uint64_t gas_limit, const Result& res);
    void check_simulate();
    int get_nonce(const address& addr, uint64_t& nonce);
private:
    evmc_message build_message(std::shared_ptr<Transaction> tx, int64_t execution_gas_limit) noexcept;
    butil::FlatMap<address, uint256be, std::hash<address>> _balence_map;
    butil::FlatMap<address, std::shared_ptr<Code>, std::hash<address>>  _code_map;
    butil::FlatMap<address, butil::FlatMap<bytes32, bytes32, std::hash<bytes32>>, std::hash<address>> _storage_map;
    butil::FlatMap<int64_t, bytes32> _block_hash;
    butil::FlatMap<address, uint64_t, std::hash<address>> _nonce_map;
    std::vector<std::shared_ptr<SimulateHost>> _hosts;
    bthread_mutex_t _mutex;
    ClientBase* _client;
    std::atomic<int> _change_idx;
    std::atomic<uint32_t>* _butex; // 1 for notice
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
    std::atomic<uint32_t>* _check_butex;
    std::shared_ptr<Code> _input;
    // for latency
    // approximate to 2 times latency between the node and client
    // in ms
    bvar::LatencyRecorder _request_latency;
    // request count in a single execution
    uint32_t _request_count;
};

}