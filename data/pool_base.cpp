#include "data/pool_base.h"
#include "data/request.h"
#include "data/tx_pool.h"
#include "data/uniswapv2.h"
#include "data/uniswapv3.h"
DECLARE_int32(long_request_failed_limit);

int get_start_block(ClientBase* client, const Address& addr, uint64_t& res) {
    uint64_t end_number = 0;
    if (request_block_number(client, end_number) != 0) {
        LOG(ERROR) << "request number error";
        return -1;
    }
    uint64_t start_number = 1;
    while (start_number <= end_number) {
        uint64_t mid = (end_number + start_number) / 2;
        DBytes code;
        if (request_code(client, addr, code, mid) != 0) {
            LOG(ERROR) << "request code failed";
            return -1;
        }
        LOG(INFO) << "code size:" << code.size() << " start:" << start_number << " end:" << end_number;
        if (code.size()) {
            res = mid;
            end_number = mid - 1;
        }
        else {
            start_number = mid + 1;
        }
    }
    return 0;
}


PoolBase::PoolBase(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg) : 
        token1(token1_arg), token2(token2_arg), address(address_arg)
{
    bthread_mutex_init(&_mutex, NULL);
}

PoolBase* PoolBase::load_from_file(std::ifstream& file) {
    PoolType type;
    file.read(reinterpret_cast<char*>(&type), sizeof(type));
    if (type == UniswapV2) {
        Address zero_addr;
        UniswapV2Pool* pool = new UniswapV2Pool(0, 0, zero_addr, 0, 0);
        file.read(reinterpret_cast<char*>(pool), sizeof(PoolBase));
        ::load_from_file(pool->_reserve0, file);
        ::load_from_file(pool->_reserve1, file);
        return pool;
    }
    else if (type == UniswapV3) {
        Address zero_addr;
        UniswapV3Pool* pool = new UniswapV3Pool(0u, 0u, zero_addr, 0, 0, 0);
        file.read(reinterpret_cast<char*>(pool), sizeof(PoolBase));
        file.read(reinterpret_cast<char*>(&pool->fee), sizeof(pool->fee));
        file.read(reinterpret_cast<char*>(&pool->tick_space), sizeof(pool->tick_space));
        ::load_from_file(pool->sqrt_price, file);
        file.read(reinterpret_cast<char*>(&pool->tick), sizeof(pool->tick));
        size_t liq_size = 0;
        file.read(reinterpret_cast<char*>(&liq_size), sizeof(size_t));
        for (size_t cur = 0; cur < liq_size; ++cur) {
            uint128_t x;
            ::load_from_file(x, file);
            pool->liquidities.push_back(x);
        }
        return pool;
    }
    else {
        LOG(ERROR) << "error type of pool";
        return NULL;
    }
}
