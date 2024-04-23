#include "data/pool_base.h"
#include "data/request.h"
#include "data/tx_pool.h"
DEFINE_int32(logs_step, 1, "requst_logs max block step");
DECLARE_int32(long_request_failed_limit);

int get_start_block(ClientBase* client, std::string addr, uint64_t& res) {
    uint64_t end_number = 0;
    if (request_block_number(client, end_number) != 0) {
        LOG(ERROR) << "request number error";
        return -1;
    }
    uint64_t start_number = 1;
    while (start_number <= end_number) {
        uint64_t mid = (end_number + start_number) / 2;
        std::string code;
        if (request_code(client, addr, code, mid) != 0) {
            LOG(ERROR) << "request code failed";
            return -1;
        }
        LOG(INFO) << "code size:" << code.size() << " start:" << start_number << " end:" << end_number;
        if (code.size() > 2) {
            res = mid;
            end_number = mid - 1;
        }
        else {
            start_number = mid + 1;
        }
    }
    return 0;
}


PoolBase::PoolBase(uint32_t token1_arg, uint32_t token2_arg, std::string address_arg) : 
        token1(token1_arg), token2(token2_arg), address(address_arg)
{
    bthread_mutex_init(&_mutex, NULL);
}

