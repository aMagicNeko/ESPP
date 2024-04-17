#include "data/tx_pool.h"
#include "data/client.h"
#include "util/json_parser.h"
#include "data/request.h"
#include "simulate/simulate_manager.h"
// 防止因一次请求失败陷入block reorg或者程序崩溃
DEFINE_int32(long_request_failed_limit, 50, "max try num in a big operation");
DEFINE_string(from, "0xa22cf23D58977639e05B45A3Db7dF6D4a91eb892", "address of from");
DECLARE_int32(batch_size);
DECLARE_bool(simulate_check);

// on a new head, to check the order of prevent txs of the pre block
void* check_order_wrap(void* arg) {
    __gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>* txs = (__gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>*)arg;
    TxPool::instance()->check_order(txs);
    delete txs;
    return NULL;
}

static std::atomic<int> get_pending_txs_wrap_status;
void* get_pending_txs_wrap(void* arg) {
    if (get_pending_txs_wrap_status.fetch_add(1, std::memory_order_relaxed) != 0) {
        LOG(INFO) << "another get_pending_txs_wrap is processing";
        get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    if (TxPool::instance()->get_pending_txs() != 0) {
        LOG(INFO) << "get pending txs failed";
        get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    ErrorHandle::instance()->pending_txs_gotten();
    get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
    return NULL;
}
// ensure only one thread is processing
static std::atomic<int> update_pools_wrap_status;

void* update_pools_wrap(void* arg) {
    auto p = (std::string*)arg;
    if (update_pools_wrap_status.fetch_add(1, std::memory_order_relaxed) != 0) {
        LOG(INFO) << "another update_pools_wrap is processing";
        update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        delete p;
        return NULL;
    }
    if (TxPool::instance()->check_parent(*p) != 0) {
        LOG(INFO) << "block reorg!";
        // block reorg
        if (TxPool::instance()->get_pools_data() != 0 ) {
            LOG(INFO) << "get pools data failed";
            delete p;
            update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
            return NULL;
        }
    }
    if (TxPool::instance()->update_pools() != 0) {
        LOG(INFO) << "update pools failed";
        delete p;
        update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    ErrorHandle::instance()->pools_updated();
    delete p;
    update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
    return NULL;
}

void TxPool::check_order(__gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>* txs) {
    std::vector<std::string> hashs;
    if (request_txs_hash_by_number(_client, 0, hashs) != 0) {
        LOG(ERROR) << "check order failed";
        return;
    }
    butil::FlatMap<std::string, uint32_t> map;
    map.init(500);
    uint32_t index = 0;
    butil::FlatMap<std::string, uint32_t> count_map;
    count_map.init(500);
    uint32_t count = 0;
    for (std::shared_ptr<Transaction> p: *txs) {
        if (count_map.seek(p->from) == 0) {
            count_map[p->from] = count++;
        }
        LOG(INFO) << "accoutn_priority_fee: " << p->account_priority_fee << " time_stamp: " << p->time_stamp << " priority_fee: " << p->priority_fee << " account: " << count_map[p->from] << " nonce: " << p->nonce; 
        map.insert(p->hash, index++);
    }
    index = 0;
    std::stringstream ss;
    uint32_t unorder = 0;
    for (std::string hash : hashs) {
        if (map.seek(hash) == 0) {
            continue;
        }
        uint32_t tmp = map[hash];
        unorder += (tmp < index) ? index - tmp : tmp - index; 
        ss << "(" << tmp << ", " << index << ")";
        ++index;
    }
    _unorder_ratio << 1.0 * unorder / map.size();
    LOG(INFO) << ss.str();
}

int TxPool::get_pending_txs() {
    std::vector<json> txs;
    if (request_txs_by_number(_client, 1, txs) != 0) {
        LOG(ERROR) << "get_pending_txs failed";
        return -1;
    }
    int32_t count = 0;
    for (auto tx : txs) {
        int ret = _client->handle_transactions(tx, 0);
        if (ret != 0) {
            LOG(ERROR) << "handle transaction failed: " <<  ret << tx.dump();
            //return -1;
        }
        ++count;
    }
    LOG(INFO) << "get pending txs: " << count;
    return 0;
}

int TxPool::on_head(const std::string& parent_hash) {
    auto tmp = new __gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>;
    {
        LockGuard lock(&_mutex);
        _txs.swap(*tmp);
        _accounts.clear();
        _txs.clear();
    }
    bthread_t bid;
    auto p = new std::string (parent_hash);
    bthread_start_background(&bid, nullptr, update_pools_wrap, p);
    bthread_start_background(&bid, nullptr, get_pending_txs_wrap, NULL);
    bthread_start_background(&bid, nullptr, check_order_wrap, tmp);
    return 0;
}

void TxPool::add_tx(std::shared_ptr<Transaction> tx) {
    LockGuard lock(&_mutex);
    if (tx->from.size() == 0) {
        return;
    }
    //LOG(INFO) << "add tx: " << tx->hash;
    auto account = _accounts.seek(tx->from);
    if (account == NULL) {
        account = _accounts.insert(tx->from, Account());
    }
    if (FLAGS_simulate_check) {
        account->nonce = tx->nonce;
        account->continuous_nonce = tx->nonce;
    }
    if (tx->nonce < account->nonce) {
        //LOG(INFO) << "old tx " << tx->hash;
        // 丢弃旧交易
        return;
    }
    auto it = account->pending_txs.find(tx->nonce);
    size_t change_idx = 10000000; // 池中最小的受到影响的tx
    if (it != account->pending_txs.end()) {
        //LOG(INFO) << "replace the same nonce " << tx->hash;
        // replace 同nonce的交易
        auto p = _txs.find(it->second);
        if (p != _txs.end()) {
            change_idx = _txs.order_of_key(*p);
            _txs.erase(it->second);
        }
        account->pending_txs.erase(it);
    }
    account->pending_txs.emplace(tx->nonce, tx);
    size_t tmp = _txs.order_of_key(tx);
    if (tmp < change_idx) {
        change_idx = tmp;
    }
    if (tx->nonce <= account->continuous_nonce) {
        // 更新同一account在这个nonce之后的交易
        update_txs(account, tx->nonce);
    }
    evmc::SimulateManager::instance()->notice_change(change_idx);
}

void TxPool::update_txs(Account* account, int64_t nonce) {
    int64_t cur = nonce;
    uint64_t prev_fee = _UINT64_MAX;
    if (cur > account->nonce) {
        prev_fee = account->pending_txs[cur - 1]->account_priority_fee;
    }
    for (auto it = account->pending_txs.find(cur); it != account->pending_txs.end(); ++it, ++cur) {
        if (it->second->nonce != cur) {
            break;
        }
        auto p = _txs.find(it->second);
        if (p != _txs.end()) {
            _txs.erase(p);
        }
        if (prev_fee > it->second->priority_fee) {
            prev_fee = it->second->priority_fee;
        }
        it->second->account_priority_fee = prev_fee;
        _txs.insert(it->second);
    }
    // 接下来的交易
    account->continuous_nonce = cur;
}

int TxPool::set_nonce(const std::string& from, uint64_t nonce) {
    LockGuard lock(&_mutex);
    auto p = _accounts.seek(from);
    if (p == NULL) {
        // 一些nonce请求在reset之前发送, 在reset之后response才收到
        //LOG(ERROR) << "Account not found: " << from;
        return 0;
    }
    p->nonce = nonce;
    p->continuous_nonce = nonce;
    update_txs(p, nonce);
    return 0;
}

bool TxPool::has_account(const std::string& from) const {
    return _accounts.seek(from) != 0;
}

int TxPool::add_pools(const std::vector<std::string>& pools) {
    std::string multicall_address = "0xcA11bde05977b3631167028862bE2a173976CA11";
    std::string muticall_head = HashAndTakeFirstFourBytes("tryAggregate(bool,(address,bytes)[])");
    std::string from = FLAGS_from;
    std::string head_token0 = HashAndTakeFirstFourBytes("token0()");
    std::string head_token1 = HashAndTakeFirstFourBytes("token1()");
    MultiCall multi_call(1);
    uint32_t j = 0; // decode call index
    for (uint32_t i = 0; i < pools.size(); ++i) {
        int failed_cnt = 0;
        Call call(pools[i], head_token0);
        multi_call.add_call(call);
        Call call1(pools[i], head_token1);
        multi_call.add_call(call1);
        if ((i + 1) % FLAGS_batch_size == 0 || i == pools.size() - 1) {
            json json_data = {
                {"jsonrpc", "2.0"},
                {"method", "eth_call"},
                {"params", {
                    {{"from", from}, {"to", multicall_address}, {"data", "0x" + muticall_head + multi_call.encode()}},
                    "latest"
                }},
                {"id", 1}
            };
            multi_call.clear();
            while (failed_cnt < FLAGS_long_request_failed_limit) {
                json json_tmp = json_data;
                if (_client->write_and_wait(json_tmp) != 0) {
                    LOG(ERROR) << "get" << i << "th UniswapV2 pool token failed";
                    ++failed_cnt;
                    continue;
                }
                std::string tmp;
                if (parse_json(json_tmp, "result", tmp) != 0) {
                    LOG(ERROR) << "call Multicall failed:" << json_tmp.dump();
                    ++failed_cnt;
                    continue;
                }
                std::vector<std::string> ress;
                if (MultiCall::decode(tmp.substr(2), ress) != 0) {
                    LOG(ERROR) << "decode multicall failed";
                    ++failed_cnt;
                    continue;
                }
                for (uint k = 0; k < ress.size(); k += 2, ++j) {
                    std::string pool_address = pools[j];
                    if (Call::decode(ress[k], tmp) != 0) {
                        LOG(ERROR) << "call failed";
                        return -1;
                    }
                    std::vector<std::string> tmp1;
                    if (DBytes::decode_32(tmp, tmp1) != 0 || tmp1.size() != 1) {
                        LOG(ERROR) << "decode dbytes failed";
                        return -1;
                    }
                    std::string token_address0 = Address::decode(tmp1[0]);

                    if (Call::decode(ress[k + 1], tmp) != 0) {
                        LOG(ERROR) << "call failed";
                        return -1;
                    }
                    if (DBytes::decode_32(tmp, tmp1) != 0 || tmp1.size() != 1) {
                        LOG(ERROR) << "decode dbytes failed";
                        return -1;
                    }
                    std::string token_address1 = Address::decode(tmp1[0]);
                    add_pool(token_address0, token_address1, pool_address);
                }
                break;
            }
        }
    }
    if (j != pools.size()) {
        LOG(ERROR) << "muticlall results size error";
        return -1;
    }
    return 0;
}

void TxPool::add_pool(const std::string& token_address0, const std::string& token_address1,
        const std::string& pool_address) {
    LockGuard lock(&_pools_mutex);
    uint32_t pool_index = _npools++;
    _pools_address.push_back(pool_address);
    _pools_index.insert(pool_address, pool_index);
    if (_tokens_index.seek(token_address0) == NULL) {
        _tokens_address.push_back(token_address0);
        _tokens_index.insert(token_address0, _ntokens);
        ++_ntokens;
        LOG(INFO) << "get token success: " << token_address0;
    }
    
    if (_tokens_index.seek(token_address1) == NULL) {
        _tokens_address.push_back(token_address1);
        _tokens_index.insert(token_address1, _ntokens);
        ++_ntokens;
        LOG(INFO) << "get token success: " << token_address1;
        LOG(INFO) << "token size" << _ntokens;
    }
    uint32_t index0 = _tokens_index[token_address0];
    uint32_t index1 = _tokens_index[token_address1];
    if (_pools.seek(index0) == NULL) {
        _pools[index0].init(1);
    }
    _pools[index0][index1].push_back(pool_index);
    if (_reverse_pools.seek(index1) == NULL) {
        _reverse_pools[index1].init(1);
    }
    _reverse_pools[index1][index0].push_back(pool_index);
}

int TxPool::init(ClientBase* client) {
    _tokens_index.init(1000);
    _pools_index.init(30000);
    _pools.init(30000);
    _reverse_pools.init(30000);
    _accounts.init(1500);
    _client = client;
    if (!FLAGS_simulate_check) {
        _dexs.push_back(new UniswapV2Abi);
    }
    _get_logs_json = {
        {"jsonrpc", "2.0"},
        {"method", "eth_getLogs"},
        {"params", 
            {
                {{"fromBlock", "0x"}}, {{"toBlock", "0x"}}, {{"topics", {}}}
            },
        },
        {"id", 1}
    };
    for (uint32_t i = 0; i < _dexs.size(); ++i) {
        _get_logs_json["params"][2]["topics"].push_back(_dexs[i]->get_logs_head());
    }
    uint32_t prev_size = 0;
    for (uint32_t i = 0; i < _dexs.size(); ++i) {
        if (_dexs[i]->init(_client) != 0) {
            return -1;
        }
        _dexs[i]->set_index(prev_size, _npools);
        prev_size = _npools;
    }
    if (get_pools_data() != 0) {
        return -1;
    }
    if (update_pools() != 0) {
        return -1;
    }
    update_pools_wrap_status.store(0);
    return 0;
}

int TxPool::get_pools_data() {
    LockGuard lock(&_pools_mutex);
    uint64_t block_num = 0;
    if (request_block_number(_client, block_num) < 0) {
        LOG(ERROR) << "get block number failed";
        return -1;
    }
    for (uint32_t i = 0; i < _dexs.size(); ++i) {
        if (_dexs[i]->get_data(_client, block_num, _pools_address) != 0) {
            LOG(ERROR) << "get pools data failed";
            return -1;
        }
    }
    _track_block = block_num;
    LOG(INFO) << "get pools data success:" << _track_block;
    return 0;
}

int TxPool::update_pools() {
    LockGuard lock(&_pools_mutex);
    int failed_cnt = 0;
    while (failed_cnt <= FLAGS_long_request_failed_limit) {
        uint64_t block_num = 0;
        if (request_block_number(_client, block_num) < 0) {
            LOG(ERROR) << "get block number failed";
            ++failed_cnt;
            continue;
        }
        if (_track_block == block_num) {
            break;
        }
        std::stringstream ss;
        ss << std::hex << _track_block + 1;
        std::string block;
        ss >> block;
        // only one block per query
        json json_data = _get_logs_json;
        json_data["params"][0]["fromBlock"] = "0x" + block;
        json_data["params"][1]["toBlock"] = "0x" + block;
        LOG(INFO) << "start to get logs:" << json_data.dump();
        if (_client->write_and_wait(json_data) != 0) {
            LOG(ERROR) << "get logs failed";
            ++failed_cnt;
            continue;
        }
        if (json_data.find("result") == json_data.end()) {
            LOG(ERROR) << "get logs failed" << json_data.dump();
            ++failed_cnt;
            continue;
        }
        for (auto &it : json_data["result"]) {
            std::string address;
            if (parse_json(it, "address", address) != 0) {
                LOG(ERROR) << "get logs failed: " << json_data.dump();
                return -1;
            }
            auto p = _pools_index.seek(address);
            if (_pools_index.seek(address) == NULL) {
                continue;
            }
            if (it.find("removed") == it.end()) {
                LOG(ERROR) << "get logs failed: " << json_data.dump();
                return -1;
            }
            if (it["removed"]) {
                continue;
            }
            uint32_t pool_index = *p;
            for (uint32_t i = 0; i < _dexs.size(); ++i) {
                if (pool_index >= _dexs[i]->start_index && pool_index < _dexs[i]->start_index + _dexs[i]->npools) {
                    //LOG(INFO) << it.dump();
                    if (_dexs[i]->on_event(pool_index, it) != 0) {
                        LOG(ERROR) << "on event failed";
                        return -1;
                    }
                    break;
                }
            }
        }
        ++_track_block;
    }
    return 0;
}

int TxPool::check_parent(const std::string& parent_hash) const {
    int failed_cnt = 0;
    std::string hash;
    while (failed_cnt <= FLAGS_long_request_failed_limit) {
        if (request_header_hash(_client, _track_block, hash) == 0) {
            break;
        }
        ++failed_cnt;
    }
    if (parent_hash != hash) {
        LOG(ERROR) << "block hash not match";
        return -1;
    }
    return 0;
}

int TxPool::get_tx(size_t index, std::shared_ptr<Transaction>& tx) {
    LockGuard lock(&_mutex);
    auto p = _txs.find_by_order(index);
    if (p == _txs.end()) {
        return -1;
    }
    tx = *p;
    return 0;
}

void TxPool::notice_simulate_result(size_t index, const std::vector<evmc::LogEntry>& logs) {

}