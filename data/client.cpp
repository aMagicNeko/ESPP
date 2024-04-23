#include "data/client.h"
#include "util/json_parser.h"
#include "data/tx_pool.h"
#include "simulate/simulate_manager.h"
DEFINE_int32(timeout_ms, 5000, "RPC timeout in milliseconds");
DEFINE_int32(wait_timeout_ms, 5000, "max wait time in milliseconds");
// avoid high-parallel error on write
DEFINE_int32(write_wait_ms, 1000, "wait before wait time in us");
DECLARE_bool(simulate_check);

const int ID_CIRCLE_SHILFT = 12;
const int INDEX_AND = ((1 << ID_CIRCLE_SHILFT) - 1);
ClientBase::ClientBase() : _tx_detail_latency("tx_detail_latency"), _header_latency("header_latency") {
    _client_fd = 0;
    _bid = 0;
    _stop = 0;
    _id.store(5); // 1 for header 2 for transaction 3 for log 4 for syncing
    _inited = 0;
    _butex_vec.resize(1 << ID_CIRCLE_SHILFT);
    for (size_t i = 0; i < _butex_vec.size(); ++i) {
        _butex_vec[i] = bthread::butex_create_checked<std::atomic<uint32_t>>();
        _butex_vec[i]->store(0);
    }
    _data_vec.resize(1 << ID_CIRCLE_SHILFT);
    _tx_send_timestamps.resize(1 << ID_CIRCLE_SHILFT);
    _nonce_send_timestamps.resize(1 << ID_CIRCLE_SHILFT);
    bthread_mutex_init(&_write_mutex, NULL);
}

int ClientBase::stop() {
    _stop = 1;
    return 0;
}

int ClientBase::set_fd(int fd) {
    _client_fd = fd;
    return 0;
}

int ClientBase::set_data(const json& j, uint32_t id) {
    uint32_t id_cur = _id.load(std::memory_order_relaxed);
    if (id + (1 << ID_CIRCLE_SHILFT) <= id_cur) {
        // old request
        return OLD_REQUEST;
    }
    _data_vec[id & ((1 << ID_CIRCLE_SHILFT) - 1)] = j;
    std::atomic_thread_fence(std::memory_order_release); // 释放屏障，确保数据写入之前的操作不会被重排序到写入之后
    return 0;
}

int ClientBase::get_data(json& j, uint32_t id) {
    uint32_t id_cur = _id.load(std::memory_order_relaxed);
    if (id + (1 << ID_CIRCLE_SHILFT) <= id_cur) {
        // old request
        return OLD_REQUEST;
    }
    std::atomic_thread_fence(std::memory_order_acquire); // 获取屏障，确保数据读取之后的操作不会被重排序到读取之前
    j = _data_vec[id & ((1 << ID_CIRCLE_SHILFT) - 1)];
    return 0;
}

int ClientBase::get_butex(std::atomic<uint32_t>** butex, uint32_t id) {
    *butex = _butex_vec[id & ((1 << ID_CIRCLE_SHILFT) - 1)];
    if ((*butex)->load(std::memory_order_relaxed) != id) {
        butex = 0;
        return OLD_REQUEST;
    }
    return 0;
}

int ClientBase::handle_headers(const json& j) {
    std::string parent_hash;
    int ret = 0;
    if (parse_json(j, "parentHash", parent_hash) != 0) {
        LOG(ERROR) << "parse timestamp failed: " << j.dump();
        ret = PARSE_HEAD_NUMBER_ERROR;
    }
    ErrorHandle::instance()->reset();
    TxPool::instance()->on_head(parent_hash);
    uint64_t timestamp = 0;
    if (parse_json(j, "timestamp", timestamp) != 0) {
        LOG(ERROR) << "parse timestamp failed: " << j.dump();
        ret = PARSE_HEAD_TIMESTAMP_ERROR;
    }
    uint64_t base_fee = 0;
    if (parse_json(j, "baseFeePerGas", base_fee) != 0) {
        LOG(ERROR) << "parse baseFeePerGas failed: " << j.dump();
        ret = PARSE_HEAD_BASE_FEE_PER_GAS_ERROR;
    }
    uint64_t number = 0;
    if (parse_json(j, "number", number) != 0) {
        LOG(ERROR) << "parse number failed: " << j.dump();
        ret = PARSE_HEAD_NUMBER_ERROR;
    }
    if (number != _block_info.number) {
        LOG(ERROR) << "head number not match: expected=" << _block_info.number << ", received=" << number;
        ret = HEAD_NUMBER_UNEPEXCED_ERROR;
    }
    uint64_t gas_limit = 0;
    if (parse_json(j, "gasLimit", gas_limit) != 0) {
        LOG(ERROR) << "parse gasLimit failed: " << j.dump();
        ret = PARSE_HEAD_GAS_LIMIT_ERROR;
    }
    uint64_t gas_used = 0;
    if (parse_json(j, "gasUsed", gas_used) != 0) {
        LOG(ERROR) << "parse gasUsed failed: " << j.dump();
        ret = PARSE_HEAD_GAS_USED_ERROR;
    }
    if (base_fee != _block_info.base_fee) {
        LOG(ERROR) << "head baseFee unexpected: expected=" << _block_info.base_fee << ", received=" << base_fee;
        ret = HEAD_BASE_FEE_UNPEXPECTED_ERROR;
    }
    uint256_t difficulty = 0;
    if (parse_json(j, "difficulty", difficulty) != 0) {
        LOG(ERROR) << "parse difficulty failed:" << j.dump();
        ret = PARSE_HEAD_TIMESTAMP_ERROR;
    }
    //BLOCK_LOG("---------------------------------------------------------------NEW BLOCK---------------");
    //BLOCK_LOG("[number=%lu][timestamp=%lu][baseFeePerGas=%lu][gasUsed=%lu][gasLimit=%lu]", number, timestamp, base_fee, gas_used, gas_limit);
    LOG(INFO) << "---------------------------------------------------------------NEW BLOCK---------------";
    LOG(INFO) << "[number=" << number << "][timestamp=" << timestamp << "][baseFeePerGas=" << base_fee << "][gasUsed=" 
            << gas_used << "][gasLimit=" << gas_limit;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    _header_latency << (now_ms - timestamp * 1000);
    uint64_t target_gas = gas_limit / 2;
    if (gas_used > target_gas) {
        _block_info.base_fee = base_fee + base_fee * (gas_used - target_gas) / (target_gas * 8);
        _block_info.gas_limit = gas_limit + gas_limit / 1024;
    }
    else if (gas_used < target_gas) {
        _block_info.base_fee = base_fee - base_fee * (target_gas - gas_used) / (target_gas * 8);
        _block_info.gas_limit = gas_limit - gas_limit / 1024;
    }
    else {
        _block_info.base_fee = base_fee;
        _block_info.gas_limit = gas_limit;
    }
    _block_info.number = number + 1;
    evmc::SimulateManager::instance()->set_head_info(timestamp + 12, _block_info.gas_limit, _block_info.base_fee, number + 1, difficulty);
    return ret;
}

int ClientBase::handle_transactions(const std::string& hash) {
    uint32_t id = _id.fetch_add(1);
    json j{{"jsonrpc", "2.0"}, {"method", "eth_getTransactionByHash"}, {"params", {hash}}, {"id", id}};
    if (write(j) != 0) {
        LOG(ERROR) << "eth_getTransactionByHash failed";
        return WRITE_GET_TRANSACTION_BY_HASH_ERROR;
    }
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    set_data(now_ms, id);
    _tx_send_timestamps[id & ((1 << ID_CIRCLE_SHILFT) - 1)] = {now_ms, hash};
    LOG(DEBUG) << "send eth_getTransactionByHash success";
    return 0;
}

int ClientBase::handle_transactions(const json& j, uint32_t id) {
    int ret = 0;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    uint64_t time_stamp = now_ms;
    std::string hash;
    if (id != 0) {
        time_stamp = _tx_send_timestamps[id & ((1 << ID_CIRCLE_SHILFT) - 1)].first;
        hash = _tx_send_timestamps[id & ((1 << ID_CIRCLE_SHILFT) - 1)].second;
        _tx_detail_latency << (now_ms - time_stamp);
        _tx_send_timestamps[id & ((1 << ID_CIRCLE_SHILFT) - 1)] = {0, ""};
    }
    do {
        std::string from;
        if (parse_json(j, "from", from)) {
            ret = PARSE_TRANSACTION_FROM_ERROR;
            break;
        }
        if (!FLAGS_simulate_check && !TxPool::instance()->has_account(from)) {
            // query nonce
            uint32_t id = _id.fetch_add(1);
            json tmp_j = {{"jsonrpc", "2.0"}, {"method", "eth_getTransactionCount"}, {"params", {j["from"], "latest"}}, {"id", id}};
            if (write(tmp_j) != 0) {
                ret = WRITE_GET_TRANSACTION_COUNT_ERROR;
                LOG(ERROR) << "eth_getTransactionCount failed";
            }
            _nonce_send_timestamps[id & INDEX_AND] = {now_ms, from};
        }
        std::string to;
        if (parse_json(j, "to", to)) {
            // new contract
        }
        uint64_t gas = 0;
        if (parse_json(j, "gas", gas) != 0) {
            ret = PARSE_TRANSACTION_GAS_ERROR;
            break;
        }
        uint64_t nonce = 0;
        if (parse_json(j, "nonce", nonce) != 0) {
            ret = PARSE_TRANSACTION_NONCE_ERROR;
            break;
        }
        uint256_t value = 0;
        if (parse_json(j, "value", value) != 0) {
            ret = PARSE_TRANSACTION_VALUE_ERROR;
            break;
        }
        std::string input;
        if (parse_json(j, "input", input) != 0) {
            ret = PARSE_TRANSACTION_INPUT_ERROR;
            break;
        }
        std::string type;
        if (parse_json(j, "type", type) != 0) {
            ret = PARSE_TRANSACTION_TYPE_ERROR;
            break;
        }
        uint64_t priority_fee = 0;
        if (type == "0x0" || type == "0x1") {
            uint64_t gas_price = 0;
            if (parse_json(j, "gasPrice", gas_price) != 0) {
                ret = PARSE_TRANSACTION_GAS_PRICE_ERROR;
                break;
            }
            if (gas_price > _block_info.base_fee) {
                priority_fee = gas_price - _block_info.base_fee;
            }
        }
        else if (type == "0x2") {
            uint64_t max_gas_price = 0;
            if (parse_json(j, "maxFeePerGas", max_gas_price) != 0) {
                ret = PARSE_TRANSACTION_MAX_FEE_PER_GAS_ERROR;
                break;
            }
            uint64_t max_priority_fee = 0;
            if (parse_json(j, "maxPriorityFeePerGas", max_priority_fee) != 0) {
                ret = PARSE_TRANSACTION_MAX_PRIORITY_FEE_PER_GAS_ERROR;
                break;
            }
            if (max_gas_price > _block_info.base_fee) {
                priority_fee = max_gas_price - _block_info.base_fee;
            }
            if (priority_fee > max_priority_fee) {
                priority_fee = max_priority_fee;
            }
        }
        else if (type == "0x3") {
            // TODO; blob transaction
        }
        else {
            ret = PARSE_TRANSACTION_TYPE_UNKNOWN_ERROR;
            break;
        }
        auto tx = std::make_shared<Transaction>(Transaction{int64_t(nonce), priority_fee, value, from, to, gas, input, 0ul, hash, time_stamp});
        if (j.find("accessList") != j.end()) {
            for (auto jj : j["accessList"]) {
                std::vector<std::string> tmp;
                tmp.push_back(jj["address"]);
                //LOG(INFO) << "add accessList:" << tmp.back();
                for (auto jjj : jj["storage"]) {
                    tmp.push_back(jjj);
                    LOG(INFO) << tmp.back();
                }
                tx->access_list.push_back(tmp);
            }
        }
        LOG(INFO) << "pending tx:" << j.dump();
        if (!FLAGS_simulate_check) [[likely]] {
            TxPool::instance()->add_tx(tx);
        }
        else [[unlikely]] {
            TxPool::instance()->add_simulate_tx(tx);
        }
        //BLOCK_LOG("pending transaction: [from=%s][nonce=%lu][priorityFee=%lu][gas=%lu][value=%s][to=%s][input=%s]", 
                //from.c_str(), nonce, priority_fee, gas, value.str().c_str(), to.c_str(), input.c_str());
        return 0;
    } while (0);

    LOG(ERROR) << "parse transaction ret: " << ret << "failed: " << j.dump();
    return ret;
}

void* ClientBase::run(void* param) {
    ClientBase *client = (ClientBase*) param;
    int timeout = FLAGS_timeout_ms;
    int client_fd = client->_client_fd;
    nlohmann::json json_data;
    int failed = 0;
    while (!client->_stop) {
        ErrorHandle::instance()->add_error(failed);
        failed = 0;
        timespec wait_abstime = butil::microseconds_to_timespec(butil::gettimeofday_us() + timeout * 1000L);
        failed = bthread_fd_timedwait(client_fd, EPOLLIN | EPOLLONESHOT, &wait_abstime);
        if (0 != failed) {
            if (errno == EWOULDBLOCK) {
                continue;
            }
            failed = BTHREAD_FDTIMED_WAIT_ERROR;
            LOG(ERROR) << "bthread_fd_timedwait failed, ret=" << failed;
            continue;
        }
        client->read(json_data);
        std::string method;
        if (parse_json(json_data, "method", method) == 0) {
            // subscirbe response
            json params;
            std::string sub_id;
            if (method == "eth_subscription" && parse_json(json_data, "params", params) == 0 && parse_json(params, "subscription", sub_id) == 0) {
                if (sub_id == client->_headers_sub_id) {
                    // new header, reset the state
                    json result;
                    if (parse_json(params, "result", result) != 0) {
                        failed = 1;
                        continue;
                    }
                    failed = client->handle_headers(result);
                }
                else if (sub_id == client->_transactions_sub_id) {
                    // new tx_hash, query tx
                    std::string result;
                    if (parse_json(params, "result", result) != 0) {
                        failed = PARSE_TRANSACTION_NO_HASH_ERROR;
                        continue;
                    }
                    failed = client->handle_transactions(result);
                }
            }
            else {
                failed = PARSE_DATA_UNKOWN_METHOD_ERROR;
                LOG(WARNING) << "unknown method: " << json_data.dump().c_str();
            }
            continue;
        }
        uint32_t id = 0;
        if (parse_json_id(json_data, "id", id) >= 0) {
            if (id == 0) {
                failed = REQUEST_ERROR;
            }
            else if (client->_tx_send_timestamps[id & ((1 << ID_CIRCLE_SHILFT) - 1)].first != 0) {
                // hash->tx response, add tx to tx_pool
                failed = client->handle_transactions(json_data["result"], id);
                continue;
            }
            if (client->_nonce_send_timestamps[id & INDEX_AND].first != 0) {
                // nonce response, set nonce to the account
                uint64_t nonce = 0;
                if (parse_json(json_data, "result", nonce) != 0) {
                    failed = PARSE_NONCE_ERROR;
                    continue;
                }
                auto now = std::chrono::system_clock::now().time_since_epoch();
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                client->_nonce_latency << (now_ms - client->_nonce_send_timestamps[id & INDEX_AND].first);
                TxPool::instance()->set_nonce(client->_nonce_send_timestamps[id & INDEX_AND].second, int64_t(nonce));
                client->_nonce_send_timestamps[id & INDEX_AND] = {0, ""};
                continue;
            }
            else {
                std::atomic<uint32_t>* butex = 0;
                if (client->get_butex(&butex, id) == 0) {
                    failed = client->set_data(json_data, id);
                    bthread::butex_wake_all(butex);
                }
                else {
                    failed = PARSE_DATA_ID_ERROR;
                    LOG(ERROR) << "unknown id: " << json_data.dump().c_str();
                }
            }
        }
        else {
            failed = PARSE_DATA_UNKOWN_TYPE_ERROR;
            LOG(WARNING) << "unknown data type: " << json_data.dump().c_str();
        }
    }
    return NULL;
}

int ClientBase::start_listen() {
    //subscribe_headers();
    //subscribe_transactions();
    // nonblock and no delay socket
    int enable = 1;
    setsockopt(_client_fd, IPPROTO_TCP, O_NDELAY, &enable, sizeof(enable));
    int flag = fcntl(_client_fd, F_GETFL);
    fcntl(_client_fd, F_SETFL, flag | O_NONBLOCK);

    if (bthread_start_background(&_bid, NULL, run, (void*) this) != 0) {
        LOG(ERROR) << "bthread_start_background failed";
        abort();
    }
    return 0;
}

int ClientBase::subscribe_headers() {
    while (true) {
        json j = {{"id", 1}, {"jsonrpc", "2.0"}, {"method", "eth_subscribe"}, {"params", {"newHeads"}}};
        uint32_t id = 0;
        if (write_and_wait(j) != 0 || parse_json_id(j, "id", id) != 0 || parse_json(j, "result", _headers_sub_id) != 0) {
            LOG(ERROR) << "subscribe headers failed";
            sleep(1);
            continue;
        }
        LOG(NOTICE) << "subscribe headers success: " << j.dump().c_str();
        return 0;
    }
}

int ClientBase::subscribe_transactions() {
    while (true) {
        json j = {{"id", 2}, {"jsonrpc", "2.0"}, {"method", "eth_subscribe"}, {"params", {"newPendingTransactions"}}};
        LOG(NOTICE) << "subscribe transactions start: " << j.dump().c_str();
        uint32_t id = 0;
        if (write_and_wait(j) != 0 || parse_json_id(j, "id", id) != 0 || parse_json(j, "result", _transactions_sub_id) != 0) {
            LOG(ERROR) << "subscribe transactions failed";
            sleep(1);
            continue;
        }
        LOG(NOTICE) << "subscribe transactions success: " << j.dump().c_str();
        return 0;
    }
}

int ClientBase::write_and_wait(json& j) {
    uint32_t id = _id.fetch_add(1, std::memory_order_relaxed);
    j["id"] = id;
    std::atomic<uint32_t>* butex = _butex_vec[id & INDEX_AND];
    butex->store(id, std::memory_order_relaxed);
    if (write(j) != 0) {
        LOG(ERROR) << "write failed";
        return -1;
    };
    timespec wait_abstime = butil::microseconds_to_timespec(butil::gettimeofday_us()
        + FLAGS_wait_timeout_ms * 1000L);
    if (bthread::butex_wait(butex, id, &wait_abstime) < 0) {
        LOG(ERROR) << "bthread::butex_wait failed";
        return -1;
    }
    if (get_data(j, id) != 0) {
        LOG(ERROR) << "get_data value failed";
        return -1;
    }
    //LOG(INFO) <<  "wait finish: " << j.dump();
    return 0;
}

int ClientBase::write(const json &param) {
    bthread_mutex_lock(&_write_mutex);
    //usleep(FLAGS_write_wait_ms);
    int ret = _write(param);
    bthread_mutex_unlock(&_write_mutex);
    return ret;
}