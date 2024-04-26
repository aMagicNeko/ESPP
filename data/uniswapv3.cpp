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
DEFINE_int32(uniswapv3_logs_step, 10000, "logs step to get pools of uniswapV3");

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
        uint64_t next = cur + FLAGS_uniswapv3_logs_step;
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
            LOG(INFO) << "matched log:" <<  log.to_string();
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
            uint64_t tick_space = log.data.to_uint256(0, 32).convert_to<uint64_t>();
            // bug of compiler, we have to allocate it on the heap
            Address pool(log.data, 44, 64);
            UniswapV3Pool* p = PoolManager::instance()->add_uniswapv3_pool(pool, token1, token2, fee, tick_space);
            if (p) {
                pools.push_back(p);
                LOG(INFO) << "UniswapV3 pool:" << p->address.to_string() << " token1:" << p->token1 << " token2" 
                        << p->token2 << " fee:" << p->fee << " tick_space:" << p->tick_space;
            }
        }
    }
    LOG(INFO) << "UniswapV3 pool size" << pools.size();
    return 0;
}

static const DBytes slot0_selector(HashAndTakeFirstFourBytes("slot0()"));
static const DBytes ticks_selector(HashAndTakeFirstFourBytes("ticks(int24)"));

int UniswapV3Pool::get_data(ClientBase* client, uint64_t block_num, std::vector<UniswapV3Pool*>& pools) {
    MultiCall multi_call(1);
    uint32_t j = 0; // cur pool data index
    // get cur tick and price
    for (uint32_t i = 0; i < pools.size(); ++i) {
        LOG(INFO) << i << ":" << pools[i]->address.to_string();
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
                LockGuard lock(&pools[j]->_mutex);
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
        for (int k = -FLAGS_uniswapv3_half_tick_count; k <= FLAGS_uniswapv3_half_tick_count; ++k) {
            multi_call.add_call(Call(pools[i]->address, ticks_selector + DBytes(Uint<256>(pools[i]->tick + k * pools[i]->tick_space).encode())));
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

int UniswapV3Pool::on_event(const LogEntry& log) {
    // memory fence is in the wrapper function
    auto p = s_pool_address_set.seek(log.address);
    if (p) {
        return 0;
    }
    LOG(INFO) << "matched uniswapV3 log, pool:" << log.address.to_string();
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
    file.write(const_cast<char*>(reinterpret_cast<const char*>(&token1)), sizeof(token1) + sizeof(token2) + sizeof(address));
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

void UniswapV3Pool::get_input_intervals(std::vector<std::pair<uint128_t, uint128_t>>& i) {
    
}

const int MAX_TICK = 887272;

// https://github.com/Uniswap/v3-core/blob/main/contracts/libraries/TickMath.sol
uint256_t getSqrtRatioAtTick(int tick) {
    uint256_t sqrtPriceX96 = 0;
    uint256_t absTick = tick < 0 ? -tick : tick;
    if (absTick > MAX_TICK) {
        return 0;
    }
    uint256_t ratio = absTick & 0x1 != 0 ? uint256_t("0xfffcb933bd6fad37aa2d162d1a594001") : uint256_t("0x100000000000000000000000000000000");
    if (absTick & 0x2 != 0) ratio = (ratio * uint256_t("0xfff97272373d413259a46990580e213a")) >> 128;
    if (absTick & 0x4 != 0) ratio = (ratio * uint256_t("0xfff2e50f5f656932ef12357cf3c7fdcc")) >> 128;
    if (absTick & 0x8 != 0) ratio = (ratio * uint256_t("0xffe5caca7e10e4e61c3624eaa0941cd0)")) >> 128;
    if (absTick & 0x10 != 0) ratio = (ratio * uint256_t("0xffcb9843d60f6159c9db58835c926644)")) >> 128;
    if (absTick & 0x20 != 0) ratio = (ratio * uint256_t("0xff973b41fa98c081472e6896dfb254c0)")) >> 128;
    if (absTick & 0x40 != 0) ratio = (ratio * uint256_t("0xff2ea16466c96a3843ec78b326b52861)")) >> 128;
    if (absTick & 0x80 != 0) ratio = (ratio * uint256_t("0xfe5dee046a99a2a811c461f1969c3053)")) >> 128;
    if (absTick & 0x100 != 0) ratio = (ratio * uint256_t("0xfcbe86c7900a88aedcffc83b479aa3a4")) >> 128;
    if (absTick & 0x200 != 0) ratio = (ratio * uint256_t("0xf987a7253ac413176f2b074cf7815e54")) >> 128;
    if (absTick & 0x400 != 0) ratio = (ratio * uint256_t("0xf3392b0822b70005940c7a398e4b70f3")) >> 128;
    if (absTick & 0x800 != 0) ratio = (ratio * uint256_t("0xe7159475a2c29b7443b29c7fa6e889d9")) >> 128;
    if (absTick & 0x1000 != 0) ratio = (ratio * uint256_t("0xd097f3bdfd2022b8845ad8f792aa5825")) >> 128;
    if (absTick & 0x2000 != 0) ratio = (ratio * uint256_t("0xa9f746462d870fdf8a65dc1f90e061e5")) >> 128;
    if (absTick & 0x4000 != 0) ratio = (ratio * uint256_t("0x70d869a156d2a1b890bb3df62baf32f7")) >> 128;
    if (absTick & 0x8000 != 0) ratio = (ratio * uint256_t("0x31be135f97d08fd981231505542fcfa6")) >> 128;
    if (absTick & 0x10000 != 0) ratio = (ratio * uint256_t("0x9aa508b5b7a84e1c677de54f3e99bc9")) >> 128;
    if (absTick & 0x20000 != 0) ratio = (ratio * uint256_t("0x5d6af8dedb81196699c329225ee604")) >> 128;
    if (absTick & 0x40000 != 0) ratio = (ratio * uint256_t("0x2216e584f5fa1ea926041bedfe98")) >> 128;
    if (absTick & 0x80000 != 0) ratio = (ratio * uint256_t("0x48a170391f7dc42444e8fa2")) >> 128;

    if (tick > 0) ratio = (std::numeric_limits<uint256_t>::max)() / ratio;

    // this divides by 1<<32 rounding up to go from a Q128.128 to a Q128.96.
    // we then downcast because we know the result always fits within 160 bits due to our tick input constraint
    // we round up in the division so getTickAtSqrtRatio of the output price is always consistent
    sqrtPriceX96 = uint160((ratio >> 32) + (ratio % (1 << 32) == 0 ? 0 : 1));
}