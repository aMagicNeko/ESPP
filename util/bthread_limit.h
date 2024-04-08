#include "common.h"
#include <deque>
class BthreadLimit {
public:
    BthreadLimit(uint32_t limit = 50) {
        _limit = limit;
        bthread_mutex_init(&_mutex, NULL);
    }
    void start_bthread(void* fn(void*), void* arg) {
        LockGuard lock(&_mutex);
        bthread_t bid;
        while (_bids.size() > _limit) {
            bid = _bids.front();
            bthread_join(bid, NULL);
            _bids.pop_front();
        }
        bthread_start_background(&bid, NULL, fn, arg);
        _bids.push_back(bid);
    }
    void end_bthread() {
        LockGuard lock(&_mutex);
        while (_bids.size() > 0) {
            bthread_join(_bids.front(), NULL);
            _bids.pop_front();
        }
    }
private:
    std::deque<bthread_t> _bids;
    bthread_mutex_t _mutex;
    uint32_t _limit;
};