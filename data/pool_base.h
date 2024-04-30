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
    static PoolBase* load_from_file(std::ifstream& file);
    const uint32_t token1;
    const uint32_t token2;
    const Address address;
    PoolBase(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg);
    PoolBase(PoolBase&) = delete;
    virtual ~PoolBase();
    virtual int on_event(const LogEntry& log, bool pending = 0) = 0;
    virtual void save_to_file(std::ofstream& file) = 0;
    virtual std::string to_string() const = 0;
    virtual PoolBase* get_copy() = 0;
    virtual int get_tick() const = 0;
    virtual uint256_t get_liquidit() const = 0;
    virtual uint256_t get_reserve0() const = 0;
    virtual uint256_t get_reserve1() const = 0;
    virtual uint32_t get_fee_rate() const = 0; // * 1e6
    // the swap funntion is smooth below the boundary 
    // lock free, only allowed used in a single thread, in the copy of the object
    virtual uint256_t get_output_boundary(uint256_t max_in, bool direction) const = 0;
    virtual uint256_t compute_output(uint256_t in, bool direction) const = 0;
    virtual uint256_t compute_input(uint256_t out, bool direction) const = 0;
    virtual uint256_t process_swap(uint256_t in, bool direction) = 0;
protected:
    bthread_mutex_t _mutex;
};
// get the create block of the contract
int get_start_block(ClientBase* client, const Address& addr, uint64_t& res);
