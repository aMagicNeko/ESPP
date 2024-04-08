#pragma once
#include "util/common.h"
#include "util/singleton.h"
#include "data/uniswapv2.h"
#include "data/error.h"
struct Transaction {
    int64_t nonce;
    uint64_t priority_fee;
    uint256_t value;
    std::string from;
    std::string to;
    uint64_t gas;
    std::string input;
    // min fee of the account of the all prev pending txs
    uint64_t account_priority_fee;
    std::string hash;
    uint64_t time_stamp;
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
        _ntokens = 0;
        _npools = 0;
        bthread_mutex_init(&_mutex, NULL);
        bthread_mutex_init(&_pools_mutex, NULL);
    }
    ~TxPool() {}
    int init(ClientBase* client);
    bool has_account(const std::string& from) const;
    int add_pools(const std::vector<std::string>& pool_address);
    // on a new head, to check the order of prevent txs of the pre block
    void check_order(std::set<std::shared_ptr<Transaction>, TxCompare>* txs);
    int get_pending_txs();
    void add_tx(std::shared_ptr<Transaction> tx);
    int set_nonce(const std::string& from, uint64_t nonce);
    int get_block_number(uint64_t& block_num) const;
    int on_head(const std::string& parent_hash);
    int check_parent(const std::string& parent_hash) const;
    int get_pools_data();
    int update_pools();
private:
    /// @brief 更新账户nonce之后的交易在txs中的位置
    void update_txs(Account* account, int64_t nonce);
    void add_pool(const std::string& token_address0, const std::string& token_address1,
                 const std::string& pool_address);
    butil::FlatMap<std::string, Account> _accounts;
    std::set<std::shared_ptr<Transaction>, TxCompare> _txs;
    uint32_t _ntokens;
    std::vector<std::string> _tokens_address;
    butil::FlatMap<std::string, uint32_t> _tokens_index;
    uint32_t _npools;
    butil::FlatMap<uint32_t, butil::FlatMap<uint32_t, std::vector<int>>> _pools;
    butil::FlatMap<uint32_t, butil::FlatMap<uint32_t, std::vector<int>>> _reverse_pools;
    butil::FlatMap<std::string, uint32_t> _pools_index;
    std::vector<std::string> _pools_address;
    ClientBase* _client;
    std::vector<AbiBase*> _dexs;
    bthread_mutex_t _mutex; // for _accounts, _txs
    bthread_mutex_t _pools_mutex; // for data of pools
    bvar::LatencyRecorder _unorder_ratio;
    uint64_t _track_block; // pools data is updated to this number
    json _get_logs_json;
};
