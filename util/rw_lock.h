#pragma once
#include <bthread/mutex.h>
#include <bthread/condition_variable.h>
#include "util/common.h"
class ReadWriteLock {
public:
    ReadWriteLock() : _readers(0), _writing(false) {
        bthread_cond_init(&_cond, NULL);
        bthread_mutex_init(&_mutex, NULL);
    }

    ~ReadWriteLock() {
        bthread_cond_destroy(&_cond);
        bthread_mutex_destroy(&_mutex);
    }

    void lock_read() {
        LockGuard lock(&_mutex);
        while (_writing) {
            bthread_cond_wait(&_cond, &_mutex);
        }
        ++_readers;
    }

    void unlock_read() {
        LockGuard lock(&_mutex);
        --_readers;
        if (_readers == 0) {
            bthread_cond_broadcast(&_cond);
        }
    }

    void lock_write() {
        LockGuard lock(&_mutex);
        while (_writing || _readers > 0) {
            bthread_cond_wait(&_cond, &_mutex);
        }
        _writing = true;
    }

    void unlock_write() {
        LockGuard lock(&_mutex);
        _writing = false;
        bthread_cond_broadcast(&_cond);
    }

private:
    bthread_mutex_t _mutex;
    bthread_cond_t _cond;
    int _readers;
    bool _writing;
};