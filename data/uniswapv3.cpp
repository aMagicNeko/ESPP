#include "data/uniswapv3.h"
#include "util/type.h"
#include "data/client.h"
#include "data/tx_pool.h"
#include "util/json_parser.h"
#include "util/bthread_limit.h"
#include "data/request.h"
#include "util/lock_guard.h"
#include "search/pool_manager.h"
DECLARE_int32(batch_size);
DECLARE_int32(bthread_limit);
DECLARE_int32(long_request_failed_limit);
DEFINE_int32(uniswapv3_half_tick_count, 6, "half of tick count to keep updating");
DECLARE_int32(logs_step);

static const Address factory_address("0x1F98431c8aD98523631AE4a59f267346ea31F984");
static std::vector<UniswapV3Pool*> s_on_event_pools;
static butil::FlatSet<Address, std::hash<Address>> s_pool_address_set;

int UniswapV3Pool::get_pools(ClientBase* client, std::vector<UniswapV3Pool*>& pools) {
    s_pool_address_set.init(1);
    uint64_t start_block = 0;
    if (get_start_block(client, factory_address, start_block) != 0) {
        LOG(ERROR) << "get pools failed";
        return -1;
    }
    LOG(INFO) << "uniswapV3 start block:" << start_block;
    uint64_t end_block = 0;
    if (request_block_number(client, end_block) != 0) {
        LOG(ERROR) << "request block number failed";
        return -1;
    }
    std::vector<Bytes32> topics;
    topics.push_back(HashAndTakeAllBytes("PoolCreated(address,address,uint24,int24,address)"));
    std::vector<LogEntry> logs;
    for (uint64_t cur = start_block; ; ) {
        uint64_t next = cur + FLAGS_logs_step;
        LOG(INFO) << "cur:" << cur << " next:" << next;
        if (next > end_block) {
            break;
        }
        for (uint32_t failed_cnt = 0; failed_cnt < FLAGS_long_request_failed_limit; ++failed_cnt) {
            logs.clear();
            if (request_filter_logs(client, cur, next - 1, topics, logs) != 0) {
                continue;
            }
            break;
        }
        cur = next;
        for (auto& log: logs) {
            if (log.address != factory_address) {
                LOG(INFO) << "not matched factory address::" << log.address.to_string();
                continue;
            }
            if (log.data.size() != 64) [[unlikely]] {
                LOG(INFO) << "wrong log data:" << log.data.to_string();
                continue;
            }
            if (log.topics.size() != 4) [[unlikely]] {
                LOG(INFO) << "wrong log topics:" << log.topics[0].to_string();
                continue;
            }
            /*
            event PoolCreated(
            address indexed token0,
            address indexed token1,
            uint24 indexed fee,
            int24 tickSpacing,
            address pool
            );
            */
            Address token1(log.topics[1]);
            Address token2(log.topics[2]);
            uint64_t fee = log.topics[3].to_uint256().convert_to<uint64_t>();
            uint64_t tick_space = log.data.to_uint256(0, 32).convert_to<uint64_t>();;
            Address pool = log.data.to_address(32, 64);
            auto p = PoolManager::instance()->add_uniswapv3_pool(pool, token1, token2, fee, tick_space);
            if (p) {
                pools.push_back(p);
            }
            // save to file
            LOG(INFO) << "UniswapV3 pool:" << pool.to_string();
        }
    }
    return 0;
}

static const std::string slot0_selector = "0x" + HashAndTakeAllBytes("slot0()");
static const std::string ticks_selector = "0x" + HashAndTakeAllBytes("ticks(int24)");

int UniswapV3Pool::get_data(ClientBase* client, uint64_t block_num, std::vector<UniswapV3Pool*>& pools) {
    MultiCall multi_call(1);
    uint32_t j = 0; // cur pool data index
    // get cur tick and price
    for (uint32_t i = 0; i < pools.size(); ++i) {
        multi_call.add_call(Call(pools[i]->address, slot0_selector));
        if ((i + 1) % FLAGS_batch_size == 0 || i == pools.size() - 1) {
            int failed_cnt = 0;
            std::vector<std::string> res;
            while (true) {
                if (failed_cnt >= FLAGS_long_request_failed_limit) {
                    return -1;
                }
                res.clear();
                if (multi_call.request_result(client, res, block_num) != 0) {
                    ++failed_cnt;
                    continue;
                }
                break;
            }
            multi_call.clear();
            for (auto& call_res : res) {
                std::vector<std::string> rs;
                if (DBytes::decode_32(call_res, rs) != 0 || rs.size() != 7) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                pools[j]->sqrt_price = Uint<256>::decode(rs[0]);
                pools[j]->tick = static_cast<uint32_t>(Uint<256>::decode(rs[1]));
                LOG(INFO) << "sqrt_price:" << pools[j]->sqrt_price << " tick:" << pools[j]->tick;
                ++j;
            }
        }
    }
    assert(j == pools.size());
    // get neighbor ticks
    j = 0;
    uint32_t batch_size = FLAGS_batch_size / (2 * FLAGS_uniswapv3_half_tick_count + 1);
    for (uint32_t i = 0; i < pools.size(); ++i) {
        for (uint k = -FLAGS_uniswapv3_half_tick_count; k <= FLAGS_uniswapv3_half_tick_count; ++k) {
            multi_call.add_call(Call(pools[i]->address, ticks_selector + Uint<256>(pools[i]->tick + k * pools[i]->tick_space).encode()));
        }
        if ((i + 1) % batch_size == 0 || i == pools.size() - 1) {
            int failed_cnt = 0;
            std::vector<std::string> res;
            while (true) {
                if (failed_cnt >= FLAGS_long_request_failed_limit) {
                    return -1;
                }
                res.clear();
                if (multi_call.request_result(client, res, block_num) != 0) {
                    ++failed_cnt;
                    continue;
                }
                break;
            }
            multi_call.clear();
            assert(res.size() % (2 * FLAGS_uniswapv3_half_tick_count + 1) == 0);
            for (uint k = 0; k < res.size(); k += 2 * FLAGS_uniswapv3_half_tick_count + 1) {
                std::vector<std::string> rs;
                LockGuard lock(&pools[j]->_mutex);
                for (uint a = k; a < (2 * FLAGS_uniswapv3_half_tick_count + 1); ++a) {
                    if (DBytes::decode_32(res[a], rs) != 0 || rs.size() != 8) {
                        LOG(ERROR) << "decode dbytes failed";
                        return -1;
                    }
                    uint128_t liquidity = Uint<128>::decode(rs[0]);
                    bool initialized = (Uint<256>::decode(rs[7]) == 0);
                    if (!initialized) {
                        liquidity = 0;
                    }
                    pools[j]->liquidities[a - k] = liquidity;
                }
                LOG(INFO) << "sqrt_price:" << pools[j]->sqrt_price << " tick:" << pools[j]->tick;
                ++j;
            }
        }
    }
    return 0;
}

void UniswapV3Pool::add_topics(std::vector<Bytes32>& topics) {
    topics.push_back(Bytes32(HashAndTakeAllBytes("Mint(address,address,int24,int24,uint128,uint256,uint256)")));
    topics.push_back(HashAndTakeAllBytes("Burn(address,int24,int24,uint128,uint256,uint256)"));
    topics.push_back(HashAndTakeAllBytes("Swap(address,address,int256,int256,uint160,uint128,int24)"));
    topics.push_back(HashAndTakeAllBytes("Flash(address,address,uint256,uint256,uint256,uint256)"));
}

UniswapV3Pool::UniswapV3Pool(uint32_t token1_arg, uint32_t token2_arg, const Address& address_arg, uint64_t fee_arg, uint64_t tick_space_arg, int tick_size) 
        :  PoolBase(token1_arg, token2_arg, address_arg), fee(fee_arg), tick_space(tick_space_arg) {
    liquidities.resize(tick_size);
}

// 直接获取tick信息
int UniswapV3Pool::on_event(const LogEntry& log) {
    auto p = s_pool_address_set.seek(log.address);
    if (p) {
        return 0;
    }
    s_on_event_pools.push_back(this);
    s_pool_address_set.insert(log.address);
    return 0;
}

int UniswapV3Pool::update_data(ClientBase* client, uint64_t block_num) {
    if (get_data(client, block_num, s_on_event_pools) != 0) {
        LOG(ERROR) << "UniswapV3 update_data failed";
        return -1;
    }
    s_on_event_pools.clear();
    s_pool_address_set.clear();
    return 0;
}

void UniswapV3Pool::save_to_file(std::ofstream& file) {
    PoolType type = UniswapV3;
    file.write(reinterpret_cast<char*>(&type), sizeof(type));
    file.write(reinterpret_cast<char*>(this), sizeof(PoolBase));
    file.write(reinterpret_cast<char*>(&fee), sizeof(fee));
    file.write(reinterpret_cast<char*>(&tick_space), sizeof(tick_space));
    ::save_to_file(sqrt_price, file);
    file.write(reinterpret_cast<char*>(&tick), sizeof(tick));
    size_t liq_size = liquidities.size();
    file.write(reinterpret_cast<char*>(&liq_size), sizeof(size_t));
    for (uint128_t x : liquidities) {
        ::save_to_file(x, file);
    }
}