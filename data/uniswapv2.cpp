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
                    ++failed_cnt;
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
                        return -1;
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
                        return -1;
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

int UniswapV2Pool::on_event(const LogEntry& log) {
    LockGuard lock(&_mutex);
    if (!(log.topics[0] == s_log_topic) || log.data.size() != 32 * 2) [[unlikely]] {
        LOG(ERROR) << "invalid swap data: " << log.data.to_string();
        return -1;
    }
    LOG(INFO) << "matched uniswapV2 log, pool:" << log.address.to_string();
    uint256_t amount0 = log.data.to_uint256(0, 32);
    uint256_t amount1 = log.data.to_uint256(32, 64);
    _reserve0 = amount0.convert_to<uint128_t>();
    _reserve1 = amount1.convert_to<uint128_t>();
    return 0;
}

void UniswapV2Pool::save_to_file(std::ofstream& file) {
    PoolType type = UniswapV2;
    file.write(reinterpret_cast<char*>(&type), sizeof(type));
    file.write(reinterpret_cast<char*>(this), sizeof(PoolBase));
    ::save_to_file(_reserve0, file);
    ::save_to_file(_reserve1, file);
}

void UniswapV2Pool::get_input_intervals(std::vector<std::pair<uint128_t, uint128_t>>& i) {
    uint128_t l = 1;
    i.emplace_back(0, (l << 112) - 1 - _reserve0);
}