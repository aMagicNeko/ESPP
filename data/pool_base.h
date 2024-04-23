#pragma once
#include "util/common.h"
#include "data/request.h"
class ClientBase;
class PoolBase {
public:
    const uint32_t token1;
    const uint32_t token2;
    const std::string address;
    PoolBase(uint32_t token1_arg, uint32_t token2_arg, std::string address_arg);
    virtual int on_event(const LogEntry& log) = 0;
    PoolBase(PoolBase&) = delete;
protected:
    bthread_mutex_t _mutex;
};

// get the create block of the contract
int get_start_block(ClientBase* client, std::string addr, uint64_t& res);
