#include "data/uniswapv2.h"
#include "util/type.h"
#include "data/client.h"
#include "data/tx_pool.h"
#include "util/json_parser.h"
#include "util/bthread_limit.h"
#include "util/lock_guard.h"
#include "search/pool_manager.h"
#include "data/request.h"
DEFINE_int32(batch_size, 500, "batch size of multicall");
DEFINE_int32(bthread_limit, 50, "bthread limit");
DECLARE_int32(long_request_failed_limit);

static const Bytes32 s_log_topic = Bytes32(HashAndTakeAllBytes("Sync(uint112,uint112)"));

int UniswapV2Pool::get_pools(ClientBase* client, std::vector<Address>& pools) {
    Address factory_address("0x5C69bEe701ef814a2B6a3EDD4B1652CB9cc5aA6f");
    std::string method = "0x" + HashAndTakeFirstFourBytes("allPairsLength()");
    DBytes result;
    if (request_call(client, factory_address, method, result) != 0) {
        LOG(ERROR) << "call allPairsLength() failed";
        return -1;
    }
    uint64_t npools = result.to_uint256(0, 32).convert_to<uint64_t>();
    LOG(INFO) << "UniswapV2 pool_num: " << npools;
    std::string head = HashAndTakeFirstFourBytes("allPairs(uint256)");
    MultiCall multi_call(1);
    for (uint64_t i = 0; i < npools; ++i) {
        Uint<256> index(i);
        Call call(factory_address, head + index.encode());
        multi_call.add_call(call);
        if ((i + 1) % FLAGS_batch_size == 0 || i == npools - 1) {
            int failed_cnt = 0;
            std::vector<std::string> res;
            while (failed_cnt < FLAGS_long_request_failed_limit) {
                res.clear();
                if (multi_call.request_result(client, res, 0) != 0) {
                    failed_cnt = 0;
                    PoolManager::instance()->reset_connection();
                    client = PoolManager::instance()->get_client();
                    continue;
                }
                break;
            }
            multi_call.clear();
            for (auto& call_res : res) {
                std::vector<std::string> rs;
                if (DBytes::decode_32(call_res, rs) != 0 || rs.size() != 1) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                Address pool_address = Address::decode(rs[0]);
                pools.push_back(pool_address);
                LOG(INFO) << "pool " << pools.size() << ": " << pool_address.to_string();
            }
            //break;
        }
    }
    LOG(INFO) << "UniswapV2 init success ";
    return 0;
}

int UniswapV2Pool::get_data(ClientBase* client, uint64_t block_num, const std::vector<Address>& pools) {
    std::vector<uint256_t> _reserve0;
    std::vector<uint256_t> _reserve1;
    std::string head = HashAndTakeFirstFourBytes("getReserves()");
    MultiCall multi_call(1);
    // get reserves
    for (uint32_t i = 0; i < pools.size(); ++i) {
        multi_call.add_call(Call(pools[i], head));
        if ((i + 1) % FLAGS_batch_size == 0 || i == pools.size() - 1) {
            std::vector<std::string> res;
            int failed_cnt = 0;
            while (true) {
                if (multi_call.request_result(client, res, block_num) != 0) {
                    if (++failed_cnt >= FLAGS_long_request_failed_limit) {
                        failed_cnt = 0;
                        PoolManager::instance()->reset_connection();
                        client = PoolManager::instance()->get_client();
                    }
                    continue;
                }
                break;
            }
            multi_call.clear();
            for (auto& call_res : res) {
                std::vector<std::string> rs;
                if (DBytes::decode_32(call_res, rs) != 0 || rs.size() != 3) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                _reserve0.push_back(Uint<256>::decode(rs[0]));
                _reserve1.push_back(Uint<256>::decode(rs[1]));
                LOG(INFO) << "reserve 0:" << _reserve0.back() << " reserve 1:" << _reserve1.back();
            }
        }
    }
    // get tokens
    std::string head_token0 = HashAndTakeFirstFourBytes("token0()");
    std::string head_token1 = HashAndTakeFirstFourBytes("token1()");
    uint32_t j = 0; // decode call index
    for (uint32_t i = 0; i < pools.size(); ++i) {
        Call call(pools[i], head_token0);
        multi_call.add_call(call);
        Call call1(pools[i], head_token1);
        multi_call.add_call(call1);
        if ((i + 1) % FLAGS_batch_size == 0 || i == pools.size() - 1) {
            std::vector<std::string> res;
            int failed_cnt = 0;
            while (true) {
                if (multi_call.request_result(client, res, block_num) != 0) {
                    if (++failed_cnt >= FLAGS_long_request_failed_limit) {
                        failed_cnt = 0;
                        PoolManager::instance()->reset_connection();
                        client = PoolManager::instance()->get_client();
                    }
                    continue;
                }
                break;
            }
            multi_call.clear();
            for (uint k = 0; k < res.size(); k += 2, ++j) {
                std::vector<std::string> rs;
                if (DBytes::decode_32(res[k], rs) != 0 || rs.size() != 1) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                Address token_address0 = Address::decode(rs[0]);
                rs.clear();
                if (DBytes::decode_32(res[k + 1], rs) != 0 || rs.size() != 1) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                Address token_address1 = Address::decode(rs[0]);
                LOG(INFO) << "token0: " << token_address0.to_string() << " token1: " << token_address1.to_string();
                PoolManager::instance()->add_uniswapv2_pool(pools[j], token_address0, token_address1, _reserve0[j], _reserve1[j]);
            }
        }
    }
    assert(j == pools.size());
    LOG(INFO) << "get UniswapV2 data success";
    return 0;
}

void UniswapV2Pool::add_topics(std::vector<Bytes32>& topics) {
    topics.push_back(s_log_topic);
}

UniswapV2Pool::UniswapV2Pool(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg, uint256_t reserve0_arg, uint256_t reserve1_arg) : PoolBase(token1_arg, token2_arg, address_arg) {
    _reserve0 = reserve0_arg.convert_to<uint128_t>();
    _reserve1 = reserve1_arg.convert_to<uint128_t>();
}

int UniswapV2Pool::on_event(const LogEntry& log, bool pending) {
    LockGuard lock(&_mutex);
    if (!(log.topics[0] == s_log_topic) || log.data.size() != 32 * 2) [[unlikely]] {
        //LOG(ERROR) << "invalid swap data: " << log.data.to_string();
        return -1; // no need to update
    }
    //LOG(INFO) << "matched uniswapV2 log, pool:" << log.address.to_string();
    uint256_t amount0 = log.data.to_uint256(0, 32);
    uint256_t amount1 = log.data.to_uint256(32, 64);
    int ret = 0;
    if (pending) {
        if (amount0 / amount1 > _reserve0 / _reserve1) {
            ret = 1; // tx process zero for one swap
        }
        else {
            ret = 0;
        }
    }
    _reserve0 = amount0.convert_to<uint128_t>();
    _reserve1 = amount1.convert_to<uint128_t>();
    return ret;
}

void UniswapV2Pool::save_to_file(std::ofstream& file) {
    PoolType type = UniswapV2;
    file.write(reinterpret_cast<char*>(&type), sizeof(type));
    file.write(const_cast<char*>(reinterpret_cast<const char*>(&token1)), sizeof(token1) + sizeof(token2) + sizeof(address));
    ::save_to_file(_reserve0, file);
    ::save_to_file(_reserve1, file);
}

uint256_t UniswapV2Pool::get_output_boundary(uint256_t max_in, bool direction) const {
    uint256_t tmp = 0;
    if (direction) {
        tmp = (uint256_t(1) << 112) - 1 - _reserve0;
    }
    else {
        tmp = (uint256_t(1) << 112) - 1 - _reserve0;
    }
    if (tmp > max_in) {
        tmp = max_in;
    }
    return compute_output(tmp, direction);
}

inline uint256_t upround_div(uint256_t x, uint256_t y) {
    return x % y ? (x / y + 1) : x / y;
}

uint256_t UniswapV2Pool::compute_output(uint256_t in, bool direction) const {
    if (in >= MAX_TOKEN_NUM || in == 0) {
        return 0;
    }
    uint256_t out = 0;
    if (direction) {
        uint256_t liq = uint256_t(1000000) * _reserve0 * _reserve1;
        //LOG(DEBUG) << "reserve0:" << _reserve0 << " reserve1:" << _reserve1 << "liq:" << liq;
        uint256_t real_in = (1000 - 3) * uint256_t(in);
        //LOG(DEBUG) << "real_in:" << real_in;
        uint256_t numof_token0_after_swap = uint256_t(1000) * _reserve0 + real_in;
        //LOG(DEBUG) << "numof_token0_after_swap:" << numof_token0_after_swap;
        uint256_t numof_token1_after_swap = upround_div(liq, numof_token0_after_swap);
        //LOG(DEBUG) << "numof_token1_after_swap:" << numof_token1_after_swap;
        out = ((uint256_t(1000) * _reserve1 - numof_token1_after_swap) / 1000);
    }
    else {
        uint256_t liq = uint256_t(1000000) * _reserve0 * _reserve1;
        //LOG(DEBUG) << "reserve0:" << _reserve0 << " reserve1:" << _reserve1 << "liq:" << liq;
        uint256_t real_in = (1000 - 3) * uint256_t(in);
        //LOG(DEBUG) << "real_in:" << real_in;
        uint256_t numof_token1_after_swap = uint256_t(1000) * _reserve1 + real_in;
        //LOG(DEBUG) << "numof_token1_after_swap:" << numof_token1_after_swap;
        uint256_t numof_token0_after_swap = upround_div(liq, numof_token1_after_swap);
        //LOG(DEBUG) << "numof_token0_after_swap:" << numof_token0_after_swap;
        out = ((uint256_t(1000) * _reserve0 - numof_token0_after_swap) / 1000);
    }
    //LOG(DEBUG) << "compute_output in:" << in << " out:" << out;
    return out;
}

uint256_t UniswapV2Pool::compute_input(uint256_t out, bool direction) const {
    if (out >= MAX_TOKEN_NUM) {
        return MAX_TOKEN_NUM + 1;
    }
    uint256_t in = 0;
    if (direction) {
        if (_reserve1 <= out) {
            return MAX_TOKEN_NUM + 1;
        }
        uint256_t liq = uint256_t(1000000) * _reserve0 * _reserve1;
        uint256_t numof_token1_after_swap = uint256_t(1000) * (_reserve1 - out);
        uint256_t numof_token0_after_swap = upround_div(liq, numof_token1_after_swap);
        //LOG(DEBUG) << "liq:" << liq << " numof_token1_after_swap:" << numof_token1_after_swap << " numof_token0_after_swap:" << numof_token0_after_swap;
        in = upround_div(numof_token0_after_swap - uint256_t(1000) * _reserve0, uint256_t(1000 - 3));
    }
    else {
        if (_reserve0 <= out) {
            return MAX_TOKEN_NUM + 1;
        }
        uint256_t liq = uint256_t(1000000) * _reserve0 * _reserve1;
        uint256_t numof_token0_after_swap = uint256_t(1000) * (_reserve0 - out);
        uint256_t numof_token1_after_swap = upround_div(liq, numof_token0_after_swap);
        //LOG(DEBUG) << "liq:" << liq << " numof_token0_after_swap:" << numof_token0_after_swap << " numof_token1_after_swap:" << numof_token1_after_swap;
        in = upround_div(numof_token1_after_swap - uint256_t(1000) * _reserve1, uint256_t(1000 - 3));
    }
    //LOG(DEBUG) << "compute_input out:" << out << " int:" << in;
    return in;
}

uint256_t UniswapV2Pool::process_swap(uint256_t in, bool direction) {
    if (in == 0 || _reserve0 == 0 || _reserve1 == 0) {
        return 0;
    }
    uint256_t out = compute_output(in, direction);
    if (in >= MAX_TOKEN_NUM || out >= MAX_TOKEN_NUM) {
        return 0;
    }
    uint128_t new_reserve0 = 0;
    uint128_t new_reserve1 = 0;
    if (direction) {
        assert(uint256_t(in) + _reserve0 < (uint256_t(1) << 128));
        new_reserve0 = in.convert_to<uint128_t>() + _reserve0;
        assert(out < _reserve1);
        new_reserve1 = _reserve1 - out.convert_to<uint128_t>();
        assert(uint256_t(new_reserve0) * new_reserve1 >= uint256_t(_reserve0) * _reserve1);
    }
    else {
        assert(uint256_t(in) + _reserve1 < (uint256_t(1) << 128));
        new_reserve1 = in.convert_to<uint128_t>() + _reserve1;
        assert(out < _reserve0);
        new_reserve0 = _reserve0 - out.convert_to<uint128_t>();
        assert(uint256_t(new_reserve0) * new_reserve1 >= uint256_t(_reserve0) * _reserve1);
    }
    _reserve0 = new_reserve0;
    _reserve1 = new_reserve1;
    //LOG(INFO) << "swap in:" << in << " out:" << out;
    return out;
}

std::string UniswapV2Pool::to_string() const {
    std::stringstream ss;
    ss << "UniswapV2 pool:" << address.to_string();
    ss << " token1:" << token1 << " token2:" << token2;
    ss << " reserve0:" << _reserve0;
    ss << " reserve1:" << _reserve1;
    return ss.str();
}

PoolBase* UniswapV2Pool::get_copy() {
    LockGuard lock(&_mutex);
    UniswapV2Pool* pool = new UniswapV2Pool(token1, token2, address, _reserve0, _reserve1);
    return pool;
}

int UniswapV2Pool::get_tick() const {
    return 0;
}

uint256_t UniswapV2Pool::get_liquidit() const {
    return uint256_t(_reserve0) * _reserve1;
}

uint256_t UniswapV2Pool::get_reserve0() const {
    return _reserve0;
}

uint256_t UniswapV2Pool::get_reserve1() const {
    return _reserve1;
}

uint32_t UniswapV2Pool::get_fee_rate() const {
    return 3000;
}