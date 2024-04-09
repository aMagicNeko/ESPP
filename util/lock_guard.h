#include "bthread/bthread.h"

class LockGuard {
public:
    LockGuard(bthread_mutex_t* mutex) {
        _mutex = mutex;
        bthread_mutex_lock(mutex);
    }
    ~LockGuard() {
        bthread_mutex_unlock(_mutex);
    }
private:
    bthread_mutex_t* _mutex;
};