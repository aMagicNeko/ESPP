#pragma once
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include "util/common.h"
#include "util/singleton.h"
#include "data/uniswapv2.h"
#include "data/error.h"
#include "simulate/simulate_host.h"
#include "simulate/evmc.hpp"
#include "util/type.h"
#include "util/transaction.h"
#include "simulate/simulate_manager.h"
#include "util/rw_lock.h"
class ClientBase;

struct Account {
    Account() {
        nonce = -1;
        continuous_nonce = -1;
    }
    int64_t nonce;
    int64_t continuous_nonce;
    std::map<int64_t, std::shared_ptr<Transaction>> pending_txs;
};

class TxCompare {
public:
    bool operator()(const std::shared_ptr<Transaction>& a, const std::shared_ptr<Transaction>& b) const {
        if (a->priority_fee == b->priority_fee) {
            if (a->from == b->from) {
                return a->nonce < b->nonce;
            }
            if (a->time_stamp == b->time_stamp) {
                return a->hash < b->hash;
            }
            return a->time_stamp < b->time_stamp;
        }
        return a->account_priority_fee > b->account_priority_fee;
    }
};

class TxPool : public Singleton<TxPool> {
public:
    TxPool() : _unorder_ratio("unorder_ratio") {
        bthread_mutex_init(&_mutex, NULL);
    }
    ~TxPool() {}
    int init(ClientBase* client);
    bool has_account(const Address& from) const;
    // on a new head, to check the order of prevent txs of the pre block
    void check_order(__gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>* txs);
    // on a new head, update txs within the tx_pool
    int get_pending_txs();
    void add_tx(std::shared_ptr<Transaction> tx);
    int set_nonce(const Address& from, uint64_t nonce);
    int on_head(uint64_t time_stamp, uint64_t gas_limit, uint256_t base_fee, uint64_t block_number, uint256_t difficulty);
    // get pending tx by its order in the pool, x is used to notice if no new tx is here
    int get_tx(size_t index, std::shared_ptr<Transaction>& tx, std::atomic<uint32_t>* x = NULL);
    void add_raw_tx(const std::string& hash, const std::string& raw_tx);
    void add_my_tx( std::shared_ptr<Transaction> tx, const Transaction& my_tx);
    std::string get_raw_tx(const std::string& hash) {
        std::string ret;
        _rw_lock.lock_read();
        auto p = _raw_txs.seek(hash);
        if (p) {
            ret = *p;
        }
        _rw_lock.unlock_read();
        return ret;
    }
private:
    /// @brief update the tx within the account
    void update_txs(Account* account, int64_t nonce);
    butil::FlatMap<Address, Account, std::hash<Address>> _accounts;
    //std::set<std::shared_ptr<Transaction>, TxCompare> _txs;
    __gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update> _txs;
    ClientBase* _client;
    butil::FlatMap<std::string, std::string> _raw_txs;
    bthread_mutex_t _mutex; // for _accounts, _txs
    bvar::LatencyRecorder _unorder_ratio;
    evmc::SimulateManager* _simulate_manager;
    ReadWriteLock _rw_lock; // for raw_tx
};