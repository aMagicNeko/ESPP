#pragma once
#include "util/common.h"
#include "data/error.h"
struct BlockInfo {
    uint64_t base_fee; // next block baseFee
    uint64_t number; // next block number
    uint64_t gas_limit; // next block gas
};

class ClientBase {
public:
    ClientBase();
    virtual ~ClientBase();
    int write(const json &param);
    int start_listen();
    int stop();
    uint32_t get_id() {
        return _id.fetch_add(1);
    }
    int write_and_wait(json& j);
    int subscribe_headers();
    int subscribe_transactions();
    int handle_transactions(const json& hash, uint32_t id = 0);
    uint64_t number() {
        // lock free
        return _block_info.number;
    }
    uint64_t base_fee() {
        // lock free
        return _block_info.base_fee;
    }
protected:
    int set_fd(int fd);
    int _client_fd;
    virtual int read(json &param) = 0;
    virtual int _write(const json &param) = 0;
private:
    //int subscribe_logs();
    int handle_transactions(const std::string& hash);
    int set_data(const json&, uint32_t id);
    int handle_headers(const json& j);
    int get_data(json&, uint32_t id);
    int get_butex(std::atomic<uint32_t>** butex, uint32_t id);
    static void* run(void* arg);
    bthread_t _bid;
    int _stop = 0;
    std::atomic<uint32_t> _id;
    std::string _transactions_sub_id;
    std::string _headers_sub_id;
    // use id to get data and butex
    std::vector<std::atomic<uint32_t>*> _butex_vec;
    std::vector<json> _data_vec;
    // time, hash
    std::vector<std::pair<uint64_t, std::string>> _tx_send_timestamps;
    // id % size -> hash
    std::vector<std::string> _raw_tx;
    // time, from
    std::vector<std::pair<uint64_t, std::string>> _nonce_send_timestamps;
    // approximate to 2 times latency between the node and client
    // in ms
    bvar::LatencyRecorder _tx_detail_latency;
    // approximate to the overall latency between the distributed system and client
    // in ms
    bvar::LatencyRecorder _header_latency;
    // approximate to 2 times latency between the node and client
    // in ms
    bvar::LatencyRecorder _nonce_latency;
    bthread_mutex_t _block_info_mutex;
    BlockInfo _block_info;
    int _inited; // not first head recieved
    bthread_mutex_t _write_mutex;
};