#pragma once
#include "util/common.h"
#define ERROR_RATE_THRESH 0.1
enum ErrorNo {
    NO_ERROR,
    PARSE_HEAD_TIMESTAMP_ERROR = 1,
    PARSE_HEAD_BASE_FEE_PER_GAS_ERROR = -2,
    PARSE_HEAD_NUMBER_ERROR = -3,
    PARSE_HEAD_GAS_LIMIT_ERROR = -4,
    PARSE_HEAD_GAS_USED_ERROR = -5,
    HEAD_NUMBER_UNEPEXCED_ERROR = -6,
    HEAD_BASE_FEE_UNPEXPECTED_ERROR = -7,
    WRITE_GET_TRANSACTION_BY_HASH_ERROR = -8,
    WRITE_GET_TRANSACTION_COUNT_ERROR = -9,
    PARSE_TRANSACTION_NO_HASH_ERROR = -10,
    PARSE_TRANSACTION_FROM_ERROR = 11,
    PARSE_TRANSACTION_GAS_ERROR = 12,
    PARSE_TRANSACTION_NONCE_ERROR = 13,
    PARSE_TRANSACTION_VALUE_ERROR = 14,
    PARSE_TRANSACTION_INPUT_ERROR = 15,
    PARSE_TRANSACTION_TYPE_ERROR = 16,
    PARSE_TRANSACTION_GAS_PRICE_ERROR = 17,
    PARSE_TRANSACTION_MAX_FEE_PER_GAS_ERROR = 18,
    PARSE_TRANSACTION_MAX_PRIORITY_FEE_PER_GAS_ERROR = 19,
    PARSE_TRANSACTION_TYPE_UNKNOWN_ERROR = 20,
    BTHREAD_FDTIMED_WAIT_ERROR = -21,
    PARSE_DATA_UNKOWN_METHOD_ERROR = 22,
    PARSE_NONCE_ERROR = 23,
    PARSE_DATA_ID_ERROR = 24,
    PARSE_DATA_UNKOWN_TYPE_ERROR = 25,
    OLD_REQUEST = 26,
    REQUEST_ERROR = 27,
};
const int ERROR_NUM = 30;
class ErrorHandle : public Singleton<ErrorHandle> {
public:
    ErrorHandle() {
        _total_cnt.store(0);
        _failed_cnt.store(0);
        _fatal.store(false);
        _pools_updated.store(false);
        _pending_txs_gotten.store(false);
    }

    void add_error(int err) {
        _total_cnt.fetch_add(1);
        if (err == 0) {
            return;
        }
        _failed_cnt.fetch_add(1);
        if (err < 0) {
            _fatal = true;
        }
    }
    // 每次新head时调用
    void reset() {
        LOG(INFO) << "total_cnt: " << _total_cnt.load() << "failed_cnt: " << _failed_cnt.load() 
                << " pools_updated: " << _pools_updated.load() << " pending_txs_gotten: " << _pending_txs_gotten.load();
        _total_cnt.store(0);
        _failed_cnt.store(0);
        _fatal.store(false);
        _pools_updated.store(false);
        _pending_txs_gotten.store(false);
    }

    bool is_fatal() const {
        float error_rate = (float)_failed_cnt.load() / _total_cnt.load();
        return _fatal.load() | (error_rate > ERROR_RATE_THRESH) | !_pools_updated.load() | !_pending_txs_gotten.load();
    }

    void pools_updated() {
        _pools_updated.store(true);
    }
    
    void pending_txs_gotten() {
        _pending_txs_gotten.store(true);
    }
private:
    std::atomic<int> _total_cnt;
    std::atomic<int> _failed_cnt;
    std::atomic<bool> _fatal;
    std::atomic<bool> _pools_updated;
    std::atomic<bool> _pending_txs_gotten;
};