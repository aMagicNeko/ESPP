#include "simulate/simulate_host.h"
#include "simulate/rlp.hpp"
#include <iterator>
#include "util/evmc_type.h"
namespace evmc {
SimulateHost::SimulateHost(VM* vm, SimulateHost* prev, const std::string& from, uint64_t nonce, const evmc_tx_context& tx_context) : 
        _vm(vm), _from(from), _nonce(nonce), _errno(0)   {
    _balance_map.init(1);
    _storage_map.init(1);
    _contract_self_destruct.init(1);
    _code_map.init(1);
    _account_access_status_set.init(1);
    _storage_access_status_map.init(1);
    _transient_storage.init(1);
    _nonce_map.init(1);
    if (prev != NULL) {
        copy_map(prev->_balance_map, _balance_map);
        copy_map(prev->_storage_map, _storage_map);
        copy_set(prev->_contract_self_destruct, _contract_self_destruct);
        copy_map(prev->_code_map, _code_map);
    }
    memcpy(&_context, &tx_context, sizeof(_context));
}

bool SimulateHost::account_exists(const address& addr) const noexcept {
    if (_contract_self_destruct.seek(addr) != 0) {
        // 已经自毁了
        return false;
    }
    auto p = _balance_map.seek(addr);
    if (p && *p) {
        return true;
    }
    uint256be balence(0);
    if (SimulateManager::instance()->get_balance(addr, balence) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
    }
    if (balence != uint256be(0)) {
        return true;
    }
    auto pp = _storage_map.seek(addr);
    if (pp) {
        return true;
    }
    auto ppp = _code_map.seek(addr);
    if (ppp) {
        return true;
    }
    size_t code_size = 0;
    SimulateManager::instance()->get_code_size(addr, code_size);
    if (code_size != 0) {
        return true;
    }
    uint64_t nonce = 0;
    SimulateManager::instance()->get_nonce(addr, code_size);
    if (nonce != 0) {
        return true;
    }
    return false;
}

bytes32 SimulateHost::get_storage(const address& addr, const bytes32& key) const noexcept {
    if (_contract_self_destruct.seek(addr) != 0) {
        // 已经自毁了
        return bytes32(0);
    }
    auto p = _storage_map.seek(addr);
    if (p) {
        auto pp = p->seek(key);
        if (pp) {
            return pp->cur;
        }
    }
    bytes32 val(0);
    if (SimulateManager::instance()->get_storage(addr, key, val) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
    }
    return val;
}

evmc_storage_status SimulateHost::set_storage(const address& addr, const bytes32& key, const bytes32& value) noexcept {
    auto p = _storage_map.seek(addr);
    if (p == 0) {
        p = _storage_map.insert(addr, butil::FlatMap<bytes32, Storage, std::hash<bytes32>>());
        p->init(1);
    }
    auto pp = p->seek(key);
    if (pp == 0) {
        bytes32 val(0);
        if (SimulateManager::instance()->get_storage(addr, key, val) != 0) {
            *const_cast<int*>(&this->_errno) = 1;
        }
        pp = p->insert(key, Storage{val, val});
    }
    evmc_storage_status ret = EVMC_STORAGE_ASSIGNED;
    bytes32 prev = pp->cur;
    bytes32 init = pp->init;
    bytes32 cur = value;
    pp->cur = value;
    if (prev == value || (prev != init && cur != init)) {
        ret = EVMC_STORAGE_ASSIGNED;
    }
    else if (init == bytes32(0) && prev == bytes32(0) && cur != bytes32(0)) {
        ret = EVMC_STORAGE_ADDED;
    }
    else if (init == prev && cur == bytes32(0)) {
        ret = EVMC_STORAGE_DELETED;
    }
    else if (init == prev && cur != bytes32(0)) {
        ret = EVMC_STORAGE_MODIFIED;
    }
    else if (init != bytes32(0) && prev == bytes32(0) && cur != bytes32(0)) {
        ret = EVMC_STORAGE_DELETED_ADDED;
    }
    else if (init != bytes32(0) && prev != bytes32(0) && init != prev && cur == bytes32(0)) {
        ret = EVMC_STORAGE_MODIFIED_DELETED;
    }
    else if (init != bytes32(0) && prev == bytes32(0) && cur == init) {
        ret = EVMC_STORAGE_DELETED_RESTORED;
    }
    else if (init == bytes32(0) && cur == bytes32(0) && prev != bytes32(0)) {
        ret = EVMC_STORAGE_ADDED_DELETED;
    }
    else if (init != bytes32(0) && prev != bytes32(0) && init != prev && cur == init) {
        ret = EVMC_STORAGE_MODIFIED_RESTORED;
    }
    if (prev != cur) {
        JournalStorageChange journal(addr, key, prev);
        _journals.push_back(journal);
    }
    return ret;
}

uint256be SimulateHost::get_balance(const address& addr) const noexcept {
    auto p = _balance_map.seek(addr);
    if (p == 0) {
        uint256be balence(0);
        if (SimulateManager::instance()->get_balance(addr, balence) != 0) {
            *const_cast<int*>(&this->_errno) = 1;
        }
        return balence;
    }
    return *p;
}

size_t SimulateHost::get_code_size(const address& addr) const noexcept {
    if (_contract_self_destruct.seek(addr) != 0) {
        // 已经自毁了
        return 0;
    }
    auto p = _code_map.seek(addr);
    if (p) {
        return (*p)->size();
    }
    size_t ret = 0;
    if (SimulateManager::instance()->get_code_size(addr, ret) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
        return 0;
    }
    return ret;
}

bytes32 SimulateHost::get_code_hash(const address& addr) const noexcept {
    if (_contract_self_destruct.seek(addr) != 0) {
        // 已经自毁了
        return bytes32(0);
    }
    auto p = _code_map.seek(addr);
    if (p) {
        return (*p)->hash();
    }
    bytes32 ret(0);
    if (SimulateManager::instance()->get_code_hash(addr, ret) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
        return bytes32(0);
    }
    return ret;
}

size_t SimulateHost::copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data, size_t buffer_size) const noexcept {
    if (_contract_self_destruct.seek(addr) != 0) {
        // 已经自毁了
        return 0;
    }
    auto p = _code_map.seek(addr);
    if (p) {
        assert((*p)->size() >= code_offset);
        size_t len = (*p)->size() - code_offset;
        if (len > buffer_size) {
            len = buffer_size;
        }
        memcpy(buffer_data, (*p)->data() + code_offset, len);
        return len;
    }
    size_t ret = 0;
    if (SimulateManager::instance()->copy_code(addr, code_offset, buffer_data, buffer_size, ret) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
    }
    return ret;
}

bool SimulateHost::selfdestruct(const evmc::address& addr, const evmc::address& beneficiary) noexcept {
    // 检查合约是否已经标记为自毁
    if (_contract_self_destruct.seek(addr) != 0) {
        return false;
    }
    // 标记该合约地址为自毁
    _contract_self_destruct.insert(addr);
    // 获取受益人和自毁合约的当前余额
    uint256be x = get_balance(beneficiary);
    uint256be y = get_balance(addr);
    // code不去显示修改 

    update_balance(addr, bytes32(0), false);
    update_balance(beneficiary, add_bytes32(x, y), false);

    JournalDestruct journal(addr);
    _journals.push_back(journal);
    return true;
}

inline void SimulateHost::update_balance(const address& address, const bytes32& value, bool add) {
    uint256be old_value = get_balance(address);
    if (add) {
        _balance_map[address] = add_bytes32(old_value, value);
    }
    else {
        _balance_map[address] = value;
    }
    JournalBalanceChange journal(address, old_value);
    _journals.push_back(journal);
}

address compute_create_address(const address& sender, uint64_t sender_nonce) noexcept
{
    const auto rlp_list = evmone::rlp::encode_tuple(sender, sender_nonce);
    const auto base_hash = str_to_bytes32(HashAndTakeAllBytes(rlp_list));
    address addr;
    std::copy_n(&base_hash.bytes[sizeof(base_hash) - sizeof(addr)], sizeof(addr), addr.bytes);
    return addr;
}

address compute_create2_address(
    const address& sender, const bytes32& salt, bytes_view init_code) noexcept
{
    const auto init_code_hash = str_to_bytes32(HashAndTakeAllBytes(init_code));
    uint8_t buffer[1 + sizeof(sender) + sizeof(salt) + sizeof(init_code_hash)];
    //static_assert(std::size(buffer) == 85);
    auto it = std::begin(buffer);
    *it++ = 0xff;
    it = std::copy_n(sender.bytes, sizeof(sender), it);
    it = std::copy_n(salt.bytes, sizeof(salt), it);
    std::copy_n(init_code_hash.bytes, sizeof(init_code_hash), it);
    const auto base_hash = str_to_bytes32(HashAndTakeAllBytes((std::basic_string_view{buffer, std::size(buffer)})));
    address addr;
    std::copy_n(&base_hash.bytes[sizeof(base_hash) - sizeof(addr)], sizeof(addr), addr.bytes);
    return addr;
}

evmc_message SimulateHost::prepare_message(evmc_message msg, uint64_t nonce)
{
    if (msg.depth == 0 || msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2)
    {
        if (msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2)
        {
            // Compute and set the address of the account being created.
            assert(msg.recipient == address{});
            assert(msg.code_address == address{});
            msg.recipient = (msg.kind == EVMC_CREATE) ?
                                compute_create_address(msg.sender, nonce) :
                                compute_create2_address(
                                    msg.sender, msg.create2_salt, bytes_view{msg.input_data, msg.input_size});
            // By EIP-2929, the access to new created address is never reverted.
            access_account(msg.recipient);
        }
    }
    return msg;
}

evmc::Result SimulateHost::create(const evmc_message& msg) noexcept
{
    assert(msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2);

    // Check collision as defined in pseudo-EIP https://github.com/ethereum/EIPs/issues/684.
    // All combinations of conditions (nonce, code, storage) are tested.
    // TODO(EVMC): Add specific error codes for creation failures.
    if (account_exists(msg.recipient))
        return evmc::Result{EVMC_FAILURE};

    uint256be x = get_balance(msg.sender);

    if (x < msg.value) {
        return evmc::Result{EVMC_FAILURE};
    }

    update_balance(msg.sender, sub_bytes32(x, msg.value), false);
    update_balance(msg.recipient, msg.value);

    auto create_msg = msg;
    const bytes_view initcode{msg.input_data, msg.input_size};
    create_msg.input_data = nullptr;
    create_msg.input_size = 0;

    auto result = _vm->execute(*this, _rev, create_msg, msg.input_data, msg.input_size);
    if (result.status_code != EVMC_SUCCESS)
    {
        result.create_address = msg.recipient;
        return result;
    }

    auto gas_left = result.gas_left;
    assert(gas_left >= 0);

    const bytes_view code{result.output_data, result.output_size};
    if (_rev >= EVMC_SPURIOUS_DRAGON && code.size() > 0x6000)
        return evmc::Result{EVMC_FAILURE};

    // Code deployment cost.
    const auto cost = std::size(code) * 200;
    gas_left -= cost;
    if (gas_left < 0)
    {
        return (_rev == EVMC_FRONTIER) ?
                   evmc::Result{EVMC_SUCCESS, result.gas_left, result.gas_refund, msg.recipient} :
                   evmc::Result{EVMC_FAILURE};
    }

    if (!code.empty() && code[0] == 0xEF)
    {
        if (_rev >= EVMC_PRAGUE)
        {
            // Only EOFCREATE/TXCREATE is allowed to deploy code starting with EF.
            assert(msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2);
            return evmc::Result{EVMC_CONTRACT_VALIDATION_FAILURE};
        }
        else if (_rev >= EVMC_LONDON)
        {
            // EIP-3541: Reject EF code.
            return evmc::Result{EVMC_CONTRACT_VALIDATION_FAILURE};
        }
    }
    _code_map[msg.recipient] = std::make_shared<Code>(code.data(), code.size());

    return evmc::Result{result.status_code, gas_left, result.gas_refund, msg.recipient};
}

evmc::Result SimulateHost::execute_message(const evmc_message& msg) noexcept
{
    if (msg.kind == EVMC_CREATE || msg.kind == EVMC_CREATE2)
        return create(msg);

    assert(msg.kind != EVMC_CALL || evmc::address{msg.recipient} == msg.code_address);

    if (msg.kind == EVMC_CALL && !evmc::is_zero(msg.value))
    {
        // Transfer value: sender → recipient.
        // The sender's balance is already checked therefore the sender account must exist.
        uint256be x = get_balance(msg.sender);
        if (x < msg.value) {
            return evmc::Result{EVMC_FAILURE};
        }
        update_balance(msg.sender, sub_bytes32(x, msg.value), false);
        update_balance(msg.recipient, msg.value);
    }
    std::shared_ptr<Code> code;
    SimulateManager::instance()->get_code(msg.code_address, &code);
    //LOG(INFO) << "msg:" << evmc_message_to_string(msg);
    //LOG(INFO) << "code:" << code->to_string();
    return _vm->execute(*this, _rev, msg, code->data(), code->size());
}

evmc::Result SimulateHost::call(const evmc_message& orig_msg) noexcept
{
    const auto msg = prepare_message(orig_msg, _nonce);

    const auto logs_checkpoint = _logs.size();
    const auto state_checkpoint = _journals.size();
    
    //LOG(INFO) << "msg:" << evmc_message_to_string(msg);

    auto result = execute_message(msg);

    if (result.status_code != EVMC_SUCCESS)
    {
        // Revert.
        roll_back(logs_checkpoint, state_checkpoint);
    }
    return result;
}

const evmc_tx_context* SimulateHost::get_tx_context() const noexcept {
    return &_context;
}

bytes32 SimulateHost::get_block_hash(int64_t block_number) const noexcept {
    bytes32 hash(0);
    if (SimulateManager::instance()->get_block_hash(block_number, hash) != 0) {
        *const_cast<int*>(&this->_errno) = 1;
    }
    return hash;
}

void SimulateHost::emit_log(const address& addr, const uint8_t* data, size_t data_size, const bytes32 topics[],
                          size_t num_topics) noexcept {
    _logs.push_back(LogEntry(addr, data, data_size, topics, num_topics));
}

bytes32 SimulateHost::get_transient_storage(const address& addr, const bytes32& key) const noexcept {
    auto pp = _transient_storage.seek(addr);
    if (pp == 0) {
        return bytes32(0);
    }
    auto ppp = pp->seek(key);
    if (ppp != 0) {
        return *ppp;
    }
    return bytes32(0);
}

void SimulateHost::set_transient_storage(const address& addr, const bytes32& key, const bytes32& value) noexcept {
    bytes32 prev(0);
    auto pp = _transient_storage.seek(addr);
    if (pp == 0) {
        pp = _transient_storage.insert(addr, butil::FlatMap<bytes32, bytes32, std::hash<bytes32>>());
        pp->init(1);
    }
    auto ppp = pp->seek(key);
    if (ppp) {
        prev = *ppp;
    }
    pp->insert(key, value);

    if (prev != value) {
        JournalTransientStorageChange journal(addr, key, prev);
        _journals.push_back(journal);
    }
}

evmc_access_status SimulateHost::access_account(const address& addr) noexcept {
    if (_account_access_status_set.seek(addr) == 0) {
        _account_access_status_set.insert(addr);
        JournalAccessAccount journal(addr);
        _journals.push_back(journal);
        return EVMC_ACCESS_COLD;
    }
    return EVMC_ACCESS_WARM;
}

evmc_access_status SimulateHost::access_storage(const address& addr, const bytes32& key) noexcept {
    auto p = _storage_access_status_map.seek(addr);
    if (p != 0 && p->seek(key) != NULL) {
        return EVMC_ACCESS_WARM;
    }
    if (p == 0) {
        p = _storage_access_status_map.insert(addr, butil::FlatSet<bytes32, std::hash<bytes32>>());
        p->init(1);
    }
    p->insert(key);
    JournalAccesssStorage journal(addr, key);
    _journals.push_back(journal);
    return EVMC_ACCESS_COLD;
}

void SimulateHost::roll_back(uint32_t nlogs, uint32_t nchanges) {
    while (_logs.size() > nlogs) {
        _logs.pop_back();
    }
    while (_journals.size() > nchanges) {
        JournalBase* journal = &_journals.back();
        if (journal->type == JournalType::BALANCE) {
            JournalBalanceChange* j = static_cast<JournalBalanceChange*>(journal);
            _balance_map[j->addr] = j->prev_balance;
        }
        else if (journal->type == JournalType::STORAGE) {
            JournalStorageChange* j = static_cast<JournalStorageChange*>(journal);
            _storage_map[j->addr][j->key].cur = j->prev_value;
        }
        else if (journal->type == JournalType::TRANSIENT_STORAGE) {
            JournalTransientStorageChange* j = static_cast<JournalTransientStorageChange*>(journal);
            _transient_storage[j->addr][j->key] = j->prev_value;
        }
        else if (journal->type == JournalType::DESTRUCT) {
            JournalDestruct* j = static_cast<JournalDestruct*>(journal);
            _contract_self_destruct.erase(j->addr);
        }
        else if (journal->type == JournalType::ACCESS_ACCOUNT) {
            JournalAccessAccount* j = static_cast<JournalAccessAccount*>(journal);
            _account_access_status_set.erase(j->addr);
        }
        else if (journal->type == JournalType::ACCESS_STORAGE) {
            JournalAccesssStorage* j = static_cast<JournalAccesssStorage*>(journal);
            auto p = _storage_access_status_map.seek(j->addr);
            if (p) {
                p->erase(j->key);
            }
        }
        _journals.pop_back();
    }
}

void SimulateHost::set_nonce(const address& addr, uint64_t nonce) {
    _nonce_map[addr] = nonce;
}

}