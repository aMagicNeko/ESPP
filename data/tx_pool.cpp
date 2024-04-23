#include "data/tx_pool.h"
#include "data/client.h"
#include "util/json_parser.h"
#include "data/request.h"
#include "simulate/simulate_manager.h"
#include "search/pool_manager.h"
// 防止因一次请求失败陷入block reorg或者程序崩溃
DEFINE_int32(long_request_failed_limit, 50, "max try num in a big operation");
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

static std::atomic<int> s_get_pending_txs_wrap_status;
void* get_pending_txs_wrap(void* arg) {
    if (s_get_pending_txs_wrap_status.fetch_add(1, std::memory_order_relaxed) != 0) {
        LOG(INFO) << "another get_pending_txs_wrap is processing";
        s_get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    if (TxPool::instance()->get_pending_txs() != 0) {
        LOG(INFO) << "get pending txs failed";
        s_get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    ErrorHandle::instance()->pending_txs_gotten();
    s_get_pending_txs_wrap_status.fetch_sub(1, std::memory_order_relaxed);
    return NULL;
}

// ensure only one thread is processing
static std::atomic<int> s_update_pools_wrap_status;

void* update_pools_wrap(void* arg) {
    auto p = (std::string*)arg;
    if (s_update_pools_wrap_status.fetch_add(1, std::memory_order_relaxed) != 0) {
        LOG(INFO) << "another update_pools_wrap is processing";
        s_update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        delete p;
        return NULL;
    }
    if (TxPool::instance()->check_parent(*p) != 0) {
        LOG(INFO) << "block reorg!";
        // block reorg
        //if (PoolManager::instance()->update_pools() != 0 ) {
        //    LOG(INFO) << "get pools data failed";
        //    delete p;
        //    s_update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        //    return NULL;
        //}
        //abort();
    }
    if (PoolManager::instance()->update_pools() != 0) {
        LOG(INFO) << "update pools failed";
        abort();
        delete p;
        s_update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
        return NULL;
    }
    ErrorHandle::instance()->pools_updated();
    delete p;
    s_update_pools_wrap_status.fetch_sub(1, std::memory_order_relaxed);
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

void TxPool::add_simulate_tx(std::shared_ptr<Transaction> tx) {
    LockGuard lock(&_mutex);
    uint32_t i = _txs.size();
    tx->account_priority_fee = 1000000 - i;
    _txs.insert(tx);
    evmc::SimulateManager::instance()->notice_change(i);
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

int TxPool::init(ClientBase* client) {
    _accounts.init(1500);
    _client = client;
    s_update_pools_wrap_status.store(0);
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

int TxPool::get_tx(size_t index, std::shared_ptr<Transaction>& tx, std::atomic<uint32_t>* x) {
    LockGuard lock(&_mutex);
    auto p = _txs.find_by_order(index);
    if (p == _txs.end()) {
        if (x) {
            x->store(0);
        }
        return -1;
    }
    tx = *p;
    return 0;
}

void TxPool::notice_simulate_result(size_t index, const std::vector<evmc::LogEntry>& logs) {

}