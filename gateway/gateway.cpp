#include "gateway/gateway.h"
#include "data/flashbot.h"
#include "search/pool_manager.h"
#include <boost/multiprecision/cpp_bin_float.hpp>
#include "data/tx_pool.h"
const Address WETH_ADDR("0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2");
const std::string relay_urls[] =
{
    "https://rpc.titanbuilder.xyz",
    "https://relay.flashbots.net/",
    "http://builder0x69.io/",
    "https://api.edennetwork.io/v1/bundle",
    "https://rpc.beaverbuild.org/",
    "https://rpc.lightspeedbuilder.info/",
    "https://eth-builder.com/",
    "https://rsync-builder.xyz/",
    "http://relayooor.wtf/",
    "http://agnostic-relay.net/",
    "http://relay.ultrasound.money/",
};


constexpr int NRELAYS = 11;
int GateWay::init(ClientBase* client) {
    _client = client;
    _from = Address("0xa22cf23D58977639e05B45A3Db7dF6D4a91eb892");
    bthread_mutex_init(&_mutex, 0);
    for (int i = 0; i < NRELAYS; ++i) {
        _bundle_clients.push_back(new FlashBotClient);
        if (_bundle_clients.back()->init(client, relay_urls[i]) != 0) {
            LOG(ERROR) << "flash bots init error";
            //return -1;
        }
    }
    return 0;
}

void GateWay::notice_search_offline_result(const SearchResult& result) {
}

struct BundlesSendArg {
    int i;
    std::vector<std::string> raw_txs;
};

void* GateWay::wrap_send_bundles(void* arg) {
    BundlesSendArg* p = (BundlesSendArg*)(arg);
    GateWay::instance()->_bundle_clients[p->i]->send_bundles(p->raw_txs);
    delete p;
    return NULL;
}

const int UNIT_COST = 130000;
const int PRIORITY_FEE = 1000000000;
void GateWay::notice_search_online_result(const SearchResult& result, std::shared_ptr<Transaction> tx) {
    {
        LockGuard lock(&_mutex);
        uint64_t base_fee = _client->base_fee();
        uint64_t fees1 = UNIT_COST * (result.direction.size() + (result.to_eth_pool ? 1 : 0)) * (base_fee + PRIORITY_FEE);
        uint64_t fees2 = UNIT_COST * (_result.direction.size() + (_result.to_eth_pool ? 1 : 0)) * (base_fee + PRIORITY_FEE);
        if (result.eth_out + fees2 > _result.eth_out + fees1) {
            _result = result;
        }
        else {
            return;
        }
    }
    std::string raw_tx;
    if (gen_tx(result, raw_tx, tx) != 0) {
        return;
    }
    std::vector<std::string> raw_txs;
    if (tx) {
        std::string ret = TxPool::instance()->get_raw_tx(tx->hash);
        if (ret.size() == 0) {
            LOG(ERROR) << "raw tx gotten for pending tx failed";
            return;
        }
        raw_txs.push_back(ret);
    }
    //raw_txs.push_back(raw_tx);
    //_bundle_clients[1]->send_bundles(raw_txs);
    //LOG(INFO) << "Received response from server " << "results" << result.to_string() <<  "my raw:" << raw_tx;
    /*
    if (_client->number()) {
        uint256_t cur = result.in;
        LOG(INFO) << "cur:" << cur;
        int i = 0;
        for (auto p : result.swap_path) {
            auto pp = PoolManager::instance()->_pools_address_map[p->address]->get_copy();
            LOG(INFO) << pp->to_string();
            cur = pp->compute_output(cur, result.direction[i]);
            LOG(INFO) << "cur:" << cur;
            ++i;
        }
        abort();
    }
    */
    for (int i = 0; i < _bundle_clients.size(); ++i) {
        bthread_t bid;
        auto p = new BundlesSendArg;
        p->raw_txs = raw_txs;
        p->i = i;
        bthread_start_background(&bid, 0, wrap_send_bundles, p);
    }
    
}

void* wrap_on_head(void*) {
    GateWay::instance()->_on_head();
    return NULL;
}

void GateWay::on_head() {
    bthread_t bid;
    bthread_start_background(&bid, 0, wrap_on_head, 0);
}

void GateWay::_on_head() {
    for (auto& flash_client : _bundle_clients) {
        flash_client->on_head();
    }
    LOG(INFO) << "on head, prev search result:" << _result.to_string();
    // TODO: trace our tx or watch others relevant tx
    _result = {};
    if (request_nonce(_client, _from, _nonce) != 0) {
        LOG(ERROR) << "on head failed";
    }
}

class SwapStep : public SolidityType {
public:
    Address pool;
    Uint<256> type;
    Address token0;
    Address token1;

    SwapStep(const Address& p, uint t, const Address& t1, const Address& t2) : pool(p), type(t), token0(t1), token1(t2) {}
    std::string encode() const {
        std::stringstream encoded;
        encoded << pool.encode();
        encoded << type.encode();
        encoded << token0.encode();
        encoded << token1.encode();
        return encoded.str();
    }
};

class MultiSteps  {
public:
    std::vector<SwapStep> steps;

    MultiSteps() {}
    void add_step(const SwapStep& step) {
        steps.push_back(step);
    }
    std::pair<std::string, std::string> encode(uint32_t start_pos = 256) const {
        std::stringstream head;
        head << std::setfill('0') << std::setw(64) << std::hex << start_pos;
        uint32_t call_size = steps.size();

        std::stringstream tail;
        // length of calls vector
        tail << std::setfill('0') << std::setw(64) << std::hex << call_size;
        for (const auto& step : steps) {
            std::string s = step.encode();
            tail << s; //tail
        }
        return {head.str(), tail.str()};
    }
};
typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<288>> float256_t;
static const std::string flash_head = HashAndTakeFirstFourBytes("startFlashLoan((address,uint256,address,address),(address,uint256,address,address)[],uint256,uint256,bytes)");
int GateWay::gen_tx(const SearchResult& result, std::string& raw_tx, std::shared_ptr<Transaction> resource_tx) {
    //LOG(INFO) << "input num:" << result.in << " eth_out:" << result.eth_out;
    //for (auto pool : result.swap_path) {
    //    LOG(INFO) << pool->to_string();
    //}
    //if (result.to_eth_pool) {
    //    LOG(INFO) << "eth path:" << result.to_eth_pool->to_string();
    //}
    Transaction tx;
    Address to("0x1c0C0075aFfFBf87C8972eF4Cc4a9FddAc46f7C5"); // contract
    tx.from = _from;
    tx.to = to;
    tx.gas = 9000000;
    tx.value = 0;
    tx.nonce = _nonce;
    tx.priority_fee = PRIORITY_FEE;
    uint64_t base_fee = _client->base_fee();
    SwapStep init_step(result.swap_path[0]->address, (int(result.direction[0]) << 1) + (result.swap_path[0]->type() == UniswapV3), PoolManager::instance()->_tokens_address[result.swap_path[0]->token1], PoolManager::instance()->_tokens_address[result.swap_path[0]->token2]);
    uint256_t input = result.in;
    MultiSteps steps;
    for (int i = 1; i < result.swap_path.size(); ++i) {
        SwapStep cur(result.swap_path[i]->address, (int(result.direction[i]) << 1) + (result.swap_path[i]->type() == UniswapV3), PoolManager::instance()->_tokens_address[result.swap_path[i]->token1], PoolManager::instance()->_tokens_address[result.swap_path[i]->token2]);
        steps.add_step(cur);
    }
    uint256_t fees = 0;
    // gas + gasPrice
    fees = UNIT_COST * result.direction.size() * (base_fee + tx.priority_fee);
    // bribe
    fees += (uint256_t(float256_t(result.eth_out) * 0.5) << 128);
    DBytes end_data;
    if (result.to_eth_pool != 0) {
        fees += 130000 * (base_fee + tx.priority_fee);
        if (fees % 2 == 0) {
            fees += 1;
        }
        SwapStep end_step(result.to_eth_pool->address, (int(result.to_eth_pool->token2) == PoolManager::instance()->weth_index() << 1) + (result.to_eth_pool->type() == UniswapV3), PoolManager::instance()->_tokens_address[result.to_eth_pool->token1], PoolManager::instance()->_tokens_address[result.to_eth_pool->token2]);
        end_data = DBytes(end_step.encode());
    }
    else {
        if (fees % 2 != 0) {
            fees += 1;
        }
    }
    auto d1 = steps.encode();
    std::string end_data_enc = end_data.encode();
    std::string end_data_head = Uint<256>((4 + 1 + 1 + 1 + 1) * 32 + d1.second.size() / 2) .encode();
    tx.input = DBytes(flash_head + init_step.encode() + d1.first + Uint<256>(input).encode() + Uint<256>(fees).encode() + end_data_head + d1.second + end_data_enc);
    // TODO: sign locally (it costs almost 0.5s to sign remotely)
    if (request_sign(_client, tx, raw_tx, base_fee) != 0) {
        return -1;
    }
    //if (resource_tx) {
    //    TxPool::instance()->add_my_tx(resource_tx, tx);
    //}
    return 0;
}

