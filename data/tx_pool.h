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
};

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
    int on_head(const std::string& parent_hash);
    int check_parent(const std::string& parent_hash) const;
    // get pending tx by its order in the pool, x is used to notice if no new tx is here
    int get_tx(size_t index, std::shared_ptr<Transaction>& tx, std::atomic<uint32_t>* x = NULL);
    void notice_simulate_result(size_t index, const std::vector<LogEntry>& logs);
    void add_simulate_tx(std::shared_ptr<Transaction> tx);
private:
    /// @brief update the tx within the account
    void update_txs(Account* account, int64_t nonce);
    butil::FlatMap<Address, Account, std::hash<Address>> _accounts;
    //std::set<std::shared_ptr<Transaction>, TxCompare> _txs;
    __gnu_pbds::tree<std::shared_ptr<Transaction>, __gnu_pbds::null_type, TxCompare,
            __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update> _txs;
    ClientBase* _client;
    bthread_mutex_t _mutex; // for _accounts, _txs
    bvar::LatencyRecorder _unorder_ratio;
    uint64_t _track_block; // pools data is updated to this number
};