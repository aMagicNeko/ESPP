#pragma once
#include "simulate/simulate_manager.h"
namespace evmc {
struct Storage {
    bytes32 init;
    bytes32 cur;
};

enum JournalType {
    BALANCE,
    STORAGE,
    TRANSIENT_STORAGE,
    CREATE,
    DESTRUCT,
    ACCESS_ACCOUNT,
    ACCESS_STORAGE
};

struct JournalBase
{
    JournalType type;
    address addr;
    JournalBase(address a, JournalType t) : type(t), addr(a) {}
};

struct JournalBalanceChange : JournalBase
{
    uint256be prev_balance;
    JournalBalanceChange(const address& a, const uint256be& v) : JournalBase(a, JournalType::BALANCE), prev_balance(v) {}
};

//struct JournalTouched : JournalBase {
//    JournalTouched() {
//        type = JournalType::BALANCE;
//    }
//};

struct JournalStorageChange : JournalBase
{
    bytes32 key;
    bytes32 prev_value;
    JournalStorageChange(const address& a, const bytes32& k, const bytes32& v) : JournalBase(a, JournalType::STORAGE), key(k), prev_value(v){}
};

struct JournalTransientStorageChange : JournalBase
{
    bytes32 key;
    bytes32 prev_value;
    JournalTransientStorageChange(const address& a, const bytes32& k, const bytes32& v) : JournalBase(a, JournalType::TRANSIENT_STORAGE), key(k), prev_value(v) {}
};

//struct JournalNonceBump : JournalBase
//{};

//struct JournalCreate : JournalBase
//{
//    bool existed;
//};

struct JournalDestruct : JournalBase {
    JournalDestruct(const address& a) : JournalBase(a, JournalType::DESTRUCT) {}
};

struct JournalAccessAccount : JournalBase {
    JournalAccessAccount(const address& a) : JournalBase(a, JournalType::ACCESS_ACCOUNT) {}
};

struct JournalAccesssStorage : JournalBase {
    bytes32 key;
    JournalAccesssStorage(const address& a, bytes32 k) : JournalBase(a, JournalType::ACCESS_STORAGE), key(k) {}
};

class SimulateHost : public Host {
public:
    virtual ~SimulateHost() noexcept = default;

    SimulateHost(VM* vm, SimulateHost* prev, const std::string& from, uint64_t nonce, const evmc_tx_context& tx_context);
    SimulateHost(const SimulateHost&) = default;
    SimulateHost(SimulateHost&&) = default;
    SimulateHost& operator=(SimulateHost&) = default;
    SimulateHost& operator=(SimulateHost&&) = default;

    evmc::Result execute_message(const evmc_message& msg) noexcept;

    /// @copydoc evmc_host_interface::account_exists
    bool account_exists(const address& addr) const noexcept override;

    /// @copydoc evmc_host_interface::get_storage
    bytes32 get_storage(const address& addr, const bytes32& key) const noexcept override;

    /// @copydoc evmc_host_interface::set_storage
    evmc_storage_status set_storage(const address& addr,
                                            const bytes32& key,
                                            const bytes32& value) noexcept override;

    /// @copydoc evmc_host_interface::get_balance
    uint256be get_balance(const address& addr) const noexcept override;

    /// @copydoc evmc_host_interface::get_code_size
    size_t get_code_size(const address& addr) const noexcept override;

    /// @copydoc evmc_host_interface::get_code_hash
    bytes32 get_code_hash(const address& addr) const noexcept override;

    /// @copydoc evmc_host_interface::copy_code
    size_t copy_code(const address& addr, size_t code_offset, uint8_t* buffer_data, size_t buffer_size) const noexcept override;

    /// @copydoc evmc_host_interface::selfdestruct
    bool selfdestruct(const address& addr, const address& beneficiary) noexcept override;

    /// @copydoc evmc_host_interface::call
    Result call(const evmc_message& msg) noexcept override;

    /// @copydoc evmc_host_interface::get_tx_context
    const evmc_tx_context* get_tx_context() const noexcept override;

    /// @copydoc evmc_host_interface::get_block_hash
    bytes32 get_block_hash(int64_t block_number) const noexcept override;

    /// @copydoc evmc_host_interface::emit_log
    void emit_log(const address& addr, const uint8_t* data, size_t data_size, const bytes32 topics[],
                          size_t num_topics) noexcept override;

    /// @copydoc evmc_host_interface::access_account
    evmc_access_status access_account(const address& addr) noexcept override;

    /// @copydoc evmc_host_interface::access_storage
    evmc_access_status access_storage(const address& addr, const bytes32& key) noexcept override;

    /// @copydoc evmc_host_interface::get_transient_storage
    bytes32 get_transient_storage(const address& addr,
                                          const bytes32& key) const noexcept override;

    /// @copydoc evmc_host_interface::set_transient_storage
    void set_transient_storage(const address& addr,
                                       const bytes32& key,
                                       const bytes32& value) noexcept override;
    bool error() const {
        return  _errno != 0;
    }
    const std::vector<LogEntry>& get_logs() {
        return _logs;
    }
    evmc_message prepare_message(evmc_message msg, uint64_t nonce);
    void set_nonce(const address& addr, uint64_t nonce);
private:
    void roll_back(uint32_t log_index, uint32_t changes_index);
    void update_balance(const address& address, const bytes32& value, bool add = true);
    evmc::Result create(const evmc_message& msg) noexcept;
    butil::FlatMap<address, uint256be, std::hash<address>> _balance_map; // exist here only if after updating
    butil::FlatMap<address, butil::FlatMap<bytes32, Storage, std::hash<bytes32>>, std::hash<address>> _storage_map; // exist here only if after updating
    butil::FlatSet<address, std::hash<address>> _contract_self_destruct; // 记录是否已经销毁
    butil::FlatMap<address, std::shared_ptr<Code>, std::hash<address>> _code_map;
    // 以下不需要从prev host继承
    butil::FlatSet<address, std::hash<address>> _account_access_status_set;
    butil::FlatMap<address, butil::FlatSet<bytes32, std::hash<bytes32>>, std::hash<address>> _storage_access_status_map;
    butil::FlatMap<address, butil::FlatMap<bytes32, bytes32, std::hash<bytes32>>, std::hash<address>> _transient_storage;
    butil::FlatMap<address, uint64_t, std::hash<address>> _nonce_map;
    VM* _vm;
    std::vector<JournalBase> _journals;
    std::vector<LogEntry> _logs;
    std::string _from; // no 0x
    uint64_t _nonce; // no 0x
    evmc_tx_context _context; //cur tx
    int _errno; // request client error
    evmc_revision _rev = EVMC_LATEST_STABLE_REVISION;
};


}