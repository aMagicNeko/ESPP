#pragma once
#include "util/common.h"
#include "data/request.h"
class ClientBase;
enum PoolType {
    UniswapV2,
    UniswapV3
};
class PoolBase {
public:
    const uint32_t token1;
    const uint32_t token2;
    const Address address;
    PoolBase(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg);
    virtual int on_event(const LogEntry& log) = 0;
    virtual void save_to_file(std::ofstream& file) = 0;
    static PoolBase* load_from_file(std::ifstream& file);
    PoolBase(PoolBase&) = delete;
protected:
    bthread_mutex_t _mutex;
};

// get the create block of the contract
int get_start_block(ClientBase* client, const Address& addr, uint64_t& res);
