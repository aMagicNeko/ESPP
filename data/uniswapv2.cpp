#include "data/uniswapv2.h"
#include "util/solidity_type.h"
#include "data/client.h"
#include "data/tx_pool.h"
#include "util/json_parser.h"
#include "util/bthread_limit.h"
DEFINE_int32(batch_size, 500, "batch size of multicall");
DEFINE_int32(bthread_limit, 50, "bthread limit");
DECLARE_string(from);
DECLARE_int32(long_request_failed_limit);

int UniswapV2Abi::init(ClientBase* client) {
    std::string factory_address = "0x5C69bEe701ef814a2B6a3EDD4B1652CB9cc5aA6f";
    std::string method = "allPairsLength()";
    std::string from = FLAGS_from;
    json json_data = {
        {"jsonrpc", "2.0"},
        {"method", "eth_call"},
        {"params", {
            {{"from", from}, {"to", factory_address}, {"data", "0x" + HashAndTakeFirstFourBytes(method)}},
            "latest"
        }},
        {"id", 1}
    };  
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "call allPairsLength failed";
        return -1;
    }
    std::string tmp;
    if (parse_json(json_data, "result", tmp) != 0) {
        LOG(ERROR) << "get allPairsLength failed";
        return -1;
    }
    npools = Uint<64>::decode(tmp.substr(2)).convert_to<uint64_t>();
    LOG(INFO) << "UniswapV2 pool_num: " << npools;
    method = "allPairs(uint256)";
    std::string head = HashAndTakeFirstFourBytes(method);
    std::string multicall_address = "0xcA11bde05977b3631167028862bE2a173976CA11";
    std::string muticall_head = HashAndTakeFirstFourBytes("tryAggregate(bool,(address,bytes)[])");
    MultiCall multi_call(0);
    //std::vector<bthread_t> bids;
    std::vector<std::string> pools;
    for (uint64_t i = 0; i < npools; ++i) {
        Uint<256> index(i);
        Call call(factory_address, head + index.encode());
        //LOG(INFO) << "songmingzhi: " << head + index.encode();
        multi_call.add_call(call);
        if ((i + 1) % FLAGS_batch_size == 0 || npools - i < uint32_t(FLAGS_batch_size)) {
            json_data = {
                {"jsonrpc", "2.0"},
                {"method", "eth_call"},
                {"params", {
                    {{"from", from}, {"to", multicall_address}, {"gas", "0xFFFFFFFFFFFFFFFF"}, {"data", "0x" + muticall_head + multi_call.encode()}},
                    "latest"
                }},
                {"id", 1}
            };
            multi_call.clear();
            int failed_cnt = 0;
            while (failed_cnt < FLAGS_long_request_failed_limit) {
                if (client->write_and_wait(json_data) != 0) {
                    LOG(ERROR) << "get" << i << "th UniswapV2 pool failed";
                    ++failed_cnt;
                    continue;
                }
                if (parse_json(json_data, "result", tmp) != 0) {
                    LOG(ERROR) << "call Multicall failed: " << json_data.dump();
                    ++failed_cnt;
                    continue;                    
                }
                std::vector<std::string> ress;
                if (MultiCall::decode(tmp.substr(2), ress) != 0) {
                    LOG(ERROR) << "decode multicall failed: " << json_data.dump();;
                    ++failed_cnt;
                    continue;
                }
                for (auto& res : ress) {
                    std::string r;
                    if (Call::decode(res, r) != 0) {
                        LOG(ERROR) << "call failed";
                        return -1;
                    }
                    std::vector<std::string> tmp1;
                    if (DBytes::decode_32(r, tmp1) != 0 || tmp1.size() != 1) {
                        LOG(ERROR) << "decode dbytes failed";
                        return -1;
                    }
                    std::string pool_address = Address::decode(tmp1[0]);
                    pools.push_back(pool_address);
                    LOG(INFO) << "pool " << pools.size() << ": " << pool_address;
                }
            }
        }
    }
    if (TxPool::instance()->add_pools(pools) != 0) {
        LOG(ERROR) << "add pools failed";
        return -1;
    }
    LOG(INFO) << "UniswapV2 init success ";
    return 0;
}

int UniswapV2Abi::get_data(ClientBase* client, uint64_t block_num, const std::vector<std::string>& pools) {
    _reserve0.clear();
    _reserve1.clear();
    std::stringstream stream;
    stream << std::hex << block_num;
    std::string block = "0x" + stream.str();
    std::string method = "getReserves()";
    std::string head = HashAndTakeFirstFourBytes(method);
    std::string multicall_address = "0xcA11bde05977b3631167028862bE2a173976CA11";
    std::string muticall_head = HashAndTakeFirstFourBytes("tryAggregate(bool,(address,bytes)[])");
    std::string from = FLAGS_from;
    MultiCall multi_call(0);
    for (uint32_t i = start_index; i < start_index + npools; ++i) {
        multi_call.add_call(Call(pools[i], head));
        if ((i + 1) % FLAGS_batch_size == 0 || i == pools.size() - 1) {
            json json_data = {
                {"jsonrpc", "2.0"},
                {"method", "eth_call"},
                {"params", {
                    {{"from", from}, {"to", multicall_address}, {"data", "0x" + muticall_head + multi_call.encode()}},
                    block
                }},
                {"id", 1}
            };
            multi_call.clear();
            if (client->write_and_wait(json_data) != 0) {
                LOG(ERROR) << "get" << i << "th UniswapV2 pool data failed";
                return -1;
            }
            std::string tmp;
            if (parse_json(json_data, "result", tmp) != 0) {
                LOG(ERROR) << "call Multicall failed: " << json_data.dump();
                return -1;
            }
            std::vector<std::string> ress;
            if (MultiCall::decode(tmp.substr(2), ress) != 0) {
                LOG(ERROR) << "decode multicall failed: " << json_data.dump();
                return -1;
            }
            for (auto& res : ress) {
                std::string r;
                if (Call::decode(res, r) != 0) {
                    LOG(ERROR) << "call failed";
                    return -1;
                }
                std::vector<std::string> tmp1;
                if (DBytes::decode_32(r, tmp1) != 0 || tmp1.size() != 3) {
                    LOG(ERROR) << "decode dbytes failed";
                    return -1;
                }
                _reserve0.push_back(Uint<256>::decode(tmp1[0]));
                _reserve1.push_back(Uint<256>::decode(tmp1[1]));
                LOG(INFO) << "reserve 0:" << _reserve0.back() << " reserve 1:" << _reserve1.back();
            }
        }
    }
    _mint_head = "0x" + HashAndTakeAllBytes("Mint(address,uint256,uint256)");
    _burn_head = "0x" + HashAndTakeAllBytes("Burn(address,uint,uint,address)");
    _swap_head = "0x" + HashAndTakeAllBytes("Swap(address,uint256,uint256,uint256,uint256,address)");
    _sync_head = "0x" + HashAndTakeAllBytes("Sync(uint112,uint112)");
    _transfer_head = "0x" + HashAndTakeAllBytes("Transfer(address,address,uint256)");
    LOG(INFO) << "get UniswapV2 data success";
    return 0;
}

int UniswapV2Abi::on_event(uint64_t pool_index_in, const json& json_data) {
    uint64_t pool_index = pool_index_in - start_index;
    if (json_data.find("topics") == json_data.end() || json_data["topics"].size() == 0 || json_data.find("data") == json_data.end()) {
        LOG(ERROR) << "invalid event data: " << json_data.dump();
        return -1;
    }
    std::string topic = json_data["topics"][0].get<std::string>();
    if (topic == _mint_head) {
        std::string data = json_data["data"];
        if (data.size() != 64 * 2 + 2) {
            LOG(ERROR) << "invalid mint data: " << json_data.dump();
            return -1;
        }
        //uint256_t amount0 = Uint<256>::decode(data.substr(2, 64));
        //uint256_t amount1 = Uint<256>::decode(data.substr(66, 64));
        //_reserve0[pool_index] += amount0;
        //_reserve1[pool_index] += amount1;
    }
    else if (topic == _burn_head) {
        std::string data = json_data["data"];
        if (data.size() != 64 * 2 + 2) {
            LOG(ERROR) << "invalid burn data: " << json_data.dump();
            return -1;
        }
        //uint256_t amount0 = Uint<256>::decode(data.substr(2, 64));
        //uint256_t amount1 = Uint<256>::decode(data.substr(66, 64));
        //_reserve0[pool_index] -= amount0;
        //_reserve1[pool_index] -= amount1;
    }
    else if (topic == _swap_head) {
        std::string data = json_data["data"];
        if (data.size() != 64 * 4 + 2) {
            LOG(ERROR) << "invalid swap data: " << json_data.dump();
            return -1;
        }
        //uint256_t amount0_in = Uint<256>::decode(data.substr(2, 64));
        //uint256_t amount1_in = Uint<256>::decode(data.substr(66, 64));
        //uint256_t amount0_out = Uint<256>::decode(data.substr(130, 64));
        //uint256_t amount1_out = Uint<256>::decode(data.substr(194, 64));
        //_reserve0[pool_index] = _reserve0[pool_index] + amount0_in - amount0_out;
        //_reserve1[pool_index] = _reserve1[pool_index] + amount1_in - amount1_out;
    }
    else if (topic == _sync_head) {
        std::string data = json_data["data"];
        if (data.size() != 64 * 2 + 2) {
            LOG(ERROR) << "invalid swap data: " << json_data.dump();
            return -1;
        }
        uint256_t amount0 = Uint<256>::decode(data.substr(2, 64));
        uint256_t amount1 = Uint<256>::decode(data.substr(66, 64));
        _reserve0[pool_index] = amount0;
        _reserve1[pool_index] = amount1;
    }
    else if (topic == _transfer_head) {

    }
    else {
        LOG(WARNING) << "invalid event topic: " << json_data.dump();
        //return -1;
    }
    return 0;
}

std::string UniswapV2Abi::get_logs_head() {
    return "0x" + HashAndTakeAllBytes("Sync(uint112,uint112)");
}