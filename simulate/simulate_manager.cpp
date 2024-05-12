#include "simulate/simulate_manager.h"
#include "data/request.h"
#include "data/tx_pool.h"
#include "util/type.h"
#include "util/evmc_type.h"
#include "util/latency.h"
#include "search/online.h"
#include <exception>
DECLARE_int32(wait_timeout_ms);
namespace evmc {
Code::Code(const std::string& str) {
    if (str.size() % 2 != 0)  {
        throw std::invalid_argument("Hex string must have an even length");
    }
    _data.resize(str.size() / 2);
    for (size_t i = 0; i < str.size(); i += 2) {
        unsigned char high = hex_char_to_byte(str[i]);
        unsigned char low = hex_char_to_byte(str[i + 1]);
        _data[i / 2] = (high << 4) | low;
    }
}

Code::Code(const uint8_t* ptr, size_t len) {
    _data.resize(len);
    for (size_t i = 0; i < len; ++i) {
        _data[i] = ptr[i];
    }
}

std::string Code::to_string() const  {
    return to_hex_string(_data.data(), _data.size());
}

SimulateManager::SimulateManager(ClientBase* client, uint64_t time_stamp, uint64_t gas_limit, uint256_t base_fee, uint64_t block_number, uint256_t difficulty) : _request_latency("request_latency") {
    _client = client;
    _balence_map.init(1000);
    _code_map.init(1000);
    _storage_map.init(1000);
    _nonce_map.init(1);
    _block_hash.init(1);
    _chain_id = bytes32(1);
    _request_block_number = 0;
    _thread_cnt.store(1); // for client thread
    _vm = new VM(evmc_create_evmone());
    bthread_mutex_init(&_balance_mutex, 0);
    bthread_mutex_init(&_code_mutex, 0);
    bthread_mutex_init(&_storage_mutex, 0);
    bthread_mutex_init(&_block_hash_mutex, 0);
    bthread_mutex_init(&_nonce_map_mutex, 0);
    _time_stamp = time_stamp;
    _gas_limit = gas_limit;
    _base_fee = uint256_to_bytes32(base_fee);
    _block_number = block_number;
    _prev_randao = uint256_to_bytes32(difficulty);
    _block_coinbase = Address("0x95222290DD7278Aa3Ddd389Cc1E1d165CC4BAfe5").value; // fake, must exist
    _blob_base_fee = _base_fee; //fake
}

SimulateManager::~SimulateManager() {
    delete _vm;
}

int SimulateManager::get_balance(const address& addr, uint256be& val) {
    LockGuard lock(&_balance_mutex);
    auto p = _balence_map.seek(addr);
    if (p == 0) {
        LatencyWrapper latency(_request_latency);
        uint256_t balance_data = 0;
        if (request_balance(_client, addr, balance_data, _request_block_number) != 0) {
            return -1;
        }
        p = _balence_map.insert(addr, uint256_to_bytes32(balance_data));
    }
    val = *p;
    return 0;
}

int SimulateManager::get_storage(const address& addr, const bytes32& key, bytes32& value) {
    LockGuard lock(&_storage_mutex);
    auto p = _storage_map.seek(addr);
    if (p == 0) {
        p = _storage_map.insert(addr, butil::FlatMap<bytes32, bytes32, std::hash<bytes32>>());
        p->init(1);
    }
    auto pp = p->seek(key);
    if (pp == 0) {
        LatencyWrapper latency(_request_latency);
        Bytes32 v;
        if (request_storage(_client, addr, key, v, _request_block_number) != 0) {
            return -1;
        }
        pp = p->insert(key, v.value);
    }
    value = *pp;
    return 0;
}

int SimulateManager::get_code(const address& addr, std::shared_ptr<Code>* c) {
    // called from external
    if (c) {
        bthread_mutex_lock(&_code_mutex);
    }
    std::string code_str;
    std::string addr_str = evmc_address_to_str(addr);
    LatencyWrapper latency(_request_latency);
    DBytes code_tmp;
    if (request_code(_client, addr, code_tmp, _request_block_number) != 0) {
        if (c) {
            bthread_mutex_unlock(&_code_mutex);
        }
        return -1;
    }
    std::shared_ptr<Code> code = std::make_shared<Code>(code_tmp._data.data(), code_tmp._data.size());
    _code_map.insert(addr, code);
    if (c) {
        // 传出
        *c = code;
        bthread_mutex_unlock(&_code_mutex);
    }
    return 0;
}

int SimulateManager::get_code_size(const address& addr, size_t& size) {
    LockGuard lock(&_code_mutex);
    auto p = _code_map.seek(addr);
    if (p == NULL) {
        if (get_code(addr) != 0) {
            size = 0;
            return -1;
        }
        p = _code_map.seek(addr);
    }
    size = (*p)->size();
    return 0;
}

int SimulateManager::get_code_hash(const address& addr, bytes32& hash) {
    LockGuard lock(&_code_mutex);
    auto p = _code_map.seek(addr);
    if (p == NULL) {
        if (get_code(addr) != 0) {
            hash = bytes32(0);
            return -1;
        }
        p = _code_map.seek(addr);
    }
    hash = (*p)->hash();
    return 0;
}

int SimulateManager::copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data, size_t buffer_size, size_t& out_size) {
    LockGuard lock(&_code_mutex);
    auto p = _code_map.seek(addr);
    if (p == NULL) {
        if (get_code(addr) != 0) {
            out_size = 0;
            return -1;
        }
        p = _code_map.seek(addr);
    }
    assert((*p)->size() >= code_offset);
    size_t len = (*p)->size() - code_offset;
    if (len > buffer_size) {
        len = buffer_size;
    }
    memcpy(buffer_data, (*p)->data() + code_offset, len);
    out_size = len;
    return 0;
}

int SimulateManager::get_block_hash(int64_t block_number, bytes32& hash) {
    LockGuard lock(&_block_hash_mutex);
    auto p = _block_hash.seek(block_number);
    if (p == 0) {
        LatencyWrapper latency(_request_latency);
        std::string hash;
        if (request_header_hash(_client, block_number, hash) != 0) {
            return -1;
        }
        hash = hash.substr(2);
        p = _block_hash.insert(block_number, str_to_bytes32(hash));
    }
    hash = *p;
    return 0;
}

int SimulateManager::get_nonce(const address& addr, uint64_t& nonce) {
    LockGuard lock(&_nonce_map_mutex);
    auto p = _nonce_map.seek(addr);
    if (p == 0) {
        LatencyWrapper latency(_request_latency);
        if (request_nonce(_client, evmc_address_to_str(addr), nonce, _request_block_number) != 0) {
            return -1;
        }
        p = _nonce_map.insert(addr, nonce);
    }
    return 0;
}

// only allowed used in a single thread
evmc_message SimulateManager::build_message(std::shared_ptr<Transaction> tx, int64_t execution_gas_limit, std::shared_ptr<Code> p) noexcept
{
    const auto recipient = tx->to.value;
    return {
        tx->to.value != address(0) ? EVMC_CALL : EVMC_CREATE,
        0,
        0,
        execution_gas_limit,
        recipient,
        tx->from.value,
        p->data(),
        p->size(),
        uint256_to_bytes32(tx->value),
        {},
        recipient,
    };
}

inline constexpr int64_t num_words(size_t size_in_bytes) noexcept
{
    return static_cast<int64_t>((size_in_bytes + 31) / 32);
}

int64_t compute_tx_data_cost(evmc_revision rev, const DBytes& input) noexcept
{
    constexpr int64_t zero_byte_cost = 4;
    const int64_t nonzero_byte_cost = rev >= EVMC_ISTANBUL ? 16 : 68;
    int64_t cost = 0;
    for (uint32_t i = 2; i + 1 < input.size(); i += 2)
        cost += (input._data[i] == 0) ? zero_byte_cost : nonzero_byte_cost;
    return cost;
}

int64_t compute_access_list_cost(const std::vector<AccessListEntry>& access_list) noexcept
{
    static constexpr auto storage_key_cost = 1900;
    static constexpr auto address_cost = 2400;

    int64_t cost = 0;
    for (const auto& a : access_list) {
        cost += address_cost + a.key.size() * storage_key_cost;
    }
    return cost;
}

int64_t compute_tx_intrinsic_cost(evmc_revision rev, std::shared_ptr<Transaction> tx) noexcept
{
    static constexpr auto call_tx_cost = 21000;
    static constexpr auto create_tx_cost = 53000;
    static constexpr auto initcode_word_cost = 2;
    const auto is_create = tx->to.value == address(0);
    const auto initcode_cost =
        is_create && rev >= EVMC_SHANGHAI ? initcode_word_cost * num_words((tx->input.size() - 2) / 2) : 0;
    const auto tx_cost = is_create && rev >= EVMC_HOMESTEAD ? create_tx_cost : call_tx_cost;
    return tx_cost + compute_tx_data_cost(rev, tx->input) + compute_access_list_cost(tx->access_list) +
           initcode_cost;
}

void SimulateManager::simulate_tx_impl(std::shared_ptr<Transaction> tx, int index) {
    address from = tx->from.value;
    const evmc_bytes32* blob_hashes = 0; // fake
    uint32_t blob_hash_count = 0; // fake
    evmc_tx_context context {
        add_bytes32(bytes32(tx->priority_fee), _base_fee),
        from,
        _block_coinbase,
        _block_number,
        _time_stamp,
        _gas_limit,
        _prev_randao,
        _chain_id,
        _base_fee,
        _blob_base_fee,
        blob_hashes,
        blob_hash_count,
        0,
        0
    };
    SimulateHost* prev = 0;
    auto host = std::make_shared<SimulateHost>(_vm, prev, tx->from, tx->nonce, context, this);
    host->set_nonce(from, tx->nonce);
    int64_t gas1 = compute_tx_intrinsic_cost(EVMC_LATEST_STABLE_REVISION, tx);
    bytes32 balance(0);
    get_balance(tx->from.value, balance);
    if (tx->gas <= static_cast<uint64_t>(gas1) || balance < uint256_to_bytes32(tx->gas * tx->priority_fee)) [[unlikely]] {
        // failed tx
        return;
    }
    std::shared_ptr<Code> p = std::make_shared<Code>(tx->input._data.data(), tx->input._data.size());
    evmc_message msg = build_message(tx, tx->gas - gas1, p);
    Result res;
    try {
        res = host->call(msg);
    }
    catch (std::exception& e) {
        LOG(WARNING) << "simulate failed:" << e.what();
        return;
    }
    LOG(INFO) << "end to simulte result:" << index << evmc_result_to_string(res);
    auto tmp = host->get_logs();
    for (auto& p:tmp) {
        LOG(INFO) << "logs:" << p.to_string();
    }
    LOG(INFO) << "gas consumption:" << gas_comsumption(tx->gas, res);
    if (host->error()) [[unlikely]] {
        LOG(ERROR) << "simulate client error";
        return;
    }
    // do arb search
    OnlineSearch search;
    search.search(tx, host->get_logs());
}

inline uint64_t SimulateManager::gas_comsumption(uint64_t gas_limit, const Result& res) {
    if (res.status_code != EVMC_SUCCESS && res.status_code != EVMC_REVERT) {
        return gas_limit;
    }
    uint64_t ret = gas_limit - res.gas_left;
    if (res.gas_refund <= static_cast<int64_t>(ret) / 2) {
        ret -= res.gas_refund;
    }
    else {
        ret -= ret / 2;
    }
    return ret;
}

struct SimulateThreadArg{
    std::shared_ptr<Transaction> tx;
    SimulateManager* manager;
};

void* SimulateManager::wrap_simulate_tx(void* arg) {
    SimulateThreadArg* p = (SimulateThreadArg*)arg;
    p->manager->simulate_tx_impl(p->tx);
    if (p->manager->_thread_cnt.fetch_sub(1) == 0) {
        delete p->manager;
    }
    delete p;
    return NULL;
}

void SimulateManager::notice_tx(std::shared_ptr<Transaction> tx) {
    SimulateThreadArg* arg = new SimulateThreadArg {tx, this};
    bthread_t bid = 0;
    _thread_cnt.fetch_add(1);
    bthread_start_background(&bid, NULL, wrap_simulate_tx, arg);
}

void SimulateManager::release_ptr() {
    if (_thread_cnt.fetch_sub(1) == 0) {
        delete this;
    }
}
}
