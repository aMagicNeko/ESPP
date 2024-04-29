#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <string_view>
#include "util/evmc_type.h"
std::string HashAndTakeFirstFourBytes(const std::string& input);
std::string HashAndTakeAllBytes(const std::string& input);
std::string HashAndTakeAllBytes(const std::basic_string<uint8_t>& input);
std::string HashAndTakeAllBytes(const std::basic_string_view<uint8_t>& input);
std::string HashAndTakeAllBytes(const std::vector<uint8_t>& input);
inline std::string vector_to_string(const std::vector<uint8_t>& data) {
    std::string result = "0x";
    result.reserve(66); // 2 characters for "0x" + 64 characters for hex

    const char hex_digits[] = "0123456789abcdef";

    for (uint8_t x:data) {
        result.push_back(hex_digits[x >> 4]);   // High nibble
        result.push_back(hex_digits[x & 0x0F]); // Low nibble
    }
    return result;
}
class SolidityType {
public:
    virtual ~SolidityType() = default;
    virtual std::string encode() const = 0;
};

template <size_t BitSize>
class Uint {
public:
    using ValueType = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<BitSize, BitSize, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
    ValueType value;
    Uint(ValueType val = 0) : value(val) {}

    std::string encode() const {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(64) << std::hex << value;
        return stream.str();
    }

    static ValueType decode(const std::string& hexStr) {
        assert(hexStr.length() == 64);
        ValueType result;
        std::stringstream ss;
        ss << std::hex << hexStr;
        ss >> result;
        return result;
    }
};

template <size_t BitSize>
class Int {
public:
    using ValueType = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<BitSize, BitSize, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
    ValueType value;
    Int(ValueType val = 0) : value(val) {}

    std::string encode() {
        std::stringstream stream;
        if (value < 0) {
            uint256_t tmp = uint256_t((-value));
            tmp = (std::numeric_limits<uint256_t>::max)() - tmp + 1;
            stream << std::setfill('f') << std::setw(64) << std::hex << tmp;
        } else {
            stream << std::setfill('0') << std::setw(64) << std::hex << value;
        }
        return stream.str();
    }

    static int256_t decode(const std::string& hexStr) {
        assert(hexStr.length() == 64);
        uint256_t result;
        std::stringstream ss;
        ss << std::hex << hexStr;
        ss >> result;

        if (result & (uint256_t(1) << 255)) {
            uint256_t twosComplement = ~result + 1;
            return int256_t(0) - twosComplement;
        } else {
            return result;
        }
    }
};

class Bytes32;
class DBytes;
class Address : public SolidityType {
public:
    evmc::address value;
    Address() : value(0) {
    }
    Address(const std::string& val) {
        if (val.size() == 0) {
            value = evmc::address(0);
        }
        value = evmc::str_to_address(val);
    }
    Address(const evmc::address& addr) : value(addr) {}
    Address(const Bytes32& b);
    Address(const DBytes& db, uint32_t l, uint32_t r);
    bool operator==(const Address& addr) const {
        return value == addr.value;
    }

    std::string encode() const override {
        std::string val = evmc::evmc_address_to_str(value);
        // padding to 32bytes
        std::string paddedValue = std::string(24, '0') + val.substr(2);
        std::transform(paddedValue.begin(), paddedValue.end(), paddedValue.begin(), ::tolower);
        return paddedValue;
    }
    // with 0x
    std::string to_string() const {
        return evmc::evmc_address_to_str(value);
    }

    static Address decode(const std::string& encodedAddress) {
        assert(encodedAddress.length() == 64);
        return Address(encodedAddress.substr(24, 40));
    }

    void save_to_file(std::ofstream& file) const {
        file.write(reinterpret_cast<const char*>(value.bytes), sizeof(value.bytes));
    }

    void load_from_file(std::ifstream& file) {
        file.read(reinterpret_cast<char*>(value.bytes), sizeof(value.bytes));
    }
};

class Bytes32 : public SolidityType {
public:
    evmc::bytes32 value;
    Bytes32() : value(0) {}
    Bytes32(const std::string& str) {
        value = evmc::str_to_bytes32(str);
    }
    Bytes32(const evmc::bytes32& val) : value(val) {}
    // without 0x
    std::string encode() const override {
        return evmc::bytes32_to_str(value).substr(2);
    }
    std::string to_string() const {
        return evmc::bytes32_to_str(value);
    }
    bool operator==(const Bytes32& other) const {
        return value == other.value;
    }
    uint256_t to_uint256() const {
        uint256_t res = 0;
        for (uint8_t x:value.bytes) {
            res <<= 8;
            res += x;
        }
        return res;
    }
};

// dynamic Bytes
class DBytes : public SolidityType {
public:
    std::vector<uint8_t> _data;
    DBytes() {}
    DBytes(const std::string& val) {
        assert(val.size() % 2 == 0);
        _data.resize(val.size() / 2);
        for (size_t i = 0; i < val.size(); i += 2) {
            unsigned char high = evmc::hex_char_to_byte(val[i]);
            unsigned char low = evmc::hex_char_to_byte(val[i + 1]);
            _data[i / 2] = (high << 4) | low;
        }
    }
    DBytes(const uint8_t* data, size_t data_size) : _data(data, data + data_size) {}
    size_t size() const {
        return _data.size();
    }
    std::string to_string() const {
        return vector_to_string(_data);
    }
    // only get tail
    std::string encode() const override {
        size_t k = _data.size();
        std::stringstream encoded;
        // length encode
        encoded << std::setfill('0') << std::setw(64) << std::hex << k;
        encoded << vector_to_string(_data).substr(2);
        // pad
        while (k % 32 != 0) {
            encoded << "00";
            ++k;
        }
        return encoded.str();
    }

    bool operator==(const DBytes& other) const {
        if (other._data.size() != _data.size()) {
            return 0;
        }
        for (uint32_t i = 0; i < _data.size(); ++i) {
            if (_data[i] != other._data[i]) {
                return 0;
            }
        }
    return 1;
    }

    DBytes operator+(const DBytes& other) const {
        DBytes ret(*this);
        for (uint8_t x:other._data) {
            ret._data.push_back(x);
        }
        return ret;
    }
    // [l,r)
    uint256_t to_uint256(size_t l, size_t r) const {
        uint256_t result = 0;
        for (size_t cur = l; cur < r; ++cur) {
            result <<= 8;
            result += _data[cur];
        }
        return result;
    }
    static int decode_32(const std::string& result, std::vector<std::string>& res) {
        std::stringstream encoded;
        encoded << result.substr(0, 64);
        uint32_t size = 0;
        encoded >> std::hex >> size;
        if (size == 0) {
            return -1; // failed
        }
        size /= 32;
        res.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            res[i] = result.substr(64 + i * 64, 64);
        }
        return 0;
    }
};

class Call : public SolidityType {
public:
    Address target;
    DBytes callData;

    // callData must have been encoded
    Call(const Address& target, const std::string& callData) : target(target), callData(callData) {}
    Call(const Address& target, const DBytes& callData) : target(target), callData(callData) {}
    std::string encode() const {
        std::stringstream encoded;

        encoded << target.encode();
        encoded << std::setfill('0') << std::setw(64) << std::hex << 2 * 32;;// head of Data(offset)
        encoded << callData.encode();
        return encoded.str();
    }
    // @param *********** (no 0x)
    static int decode(const std::string& result, std::string& res) {
        std::stringstream encoded;
        int32_t index = 0;
        // bool success
        encoded << result.substr(index, 64);
        index += 64;
        uint32_t success;
        encoded >> std::hex >> success;
        if (success == 0) {
            return -1; // failed
        }
        // bool + data offset
        res = result.substr(128);
        //LOG(INFO) << 128 << " " << res;
        return 0;
    }
};

class ClientBase;

class MultiCall : public SolidityType {
private:
    std::vector<Call> calls;
    bool requireSuccess;

public:
    MultiCall(bool requireSuccess) : requireSuccess(requireSuccess) {}

    void add_call(const Call& call) {
        calls.push_back(call);
    }

    size_t size() const {
        return calls.size();
    }
    
    std::string encode() const override {
        std::stringstream encoded;
        
        // bool requireSuccess
        encoded << std::setfill('0') << std::setw(64) << (requireSuccess ? 1 : 0);
        
        // pos of d-size data
        encoded << std::setfill('0') << std::setw(64) << std::hex << 2 * 32;

        uint32_t call_size = calls.size();
        // length of calls vector
        encoded << std::setfill('0') << std::setw(64) << std::hex << call_size;

        std::stringstream tail;
        uint32_t len = 0; // accumulated tail len (32 bytes as a unit)

        for (const auto& call : calls) {
            std::string s = call.encode();
            if (s.size() % 64 != 0) {
                LOG(ERROR) << "Call encode size is not a multiple of 32:" << s;
                break;
            }
            // Head(Call)
            encoded << std::setfill('0') << std::setw(64) << std::hex << (len + call_size) * 32;
            len += s.size() / 64;
            tail << s; //tail
        }

        return encoded.str() + tail.str();
    }

    void clear() {
        calls.clear();
    }

    // every call must succeed
    // @param res: bytes data of each call unit
    int request_result(ClientBase* client, std::vector<std::string>& res, uint64_t block_num);
    // @return {bool(success), bytes(data)}
    // @param *****
    static int decode(const std::string& result, std::vector<std::string>& res) {
        // the offset of the whole vector
        if (result.size() < 64) {
            return -1;
        }
        uint32_t index = 64;
        std::stringstream encoded;
        // sizeof(Results)
        encoded << result.substr(index, 64);
        index += 64;
        uint32_t size = 0;
        encoded >> std::hex >> size;
        res.resize(size);
        uint32_t prev_offset = 0;
        for (uint32_t j = 0; j < size; ++j) {
            encoded.clear();
            // head of Result[i] (offset)
            encoded << result.substr(index, 64);
            uint32_t offset = 0;
            encoded >> std::hex >> offset; 
            if (prev_offset) {
                res[j-1] = result.substr(128 + 2 * prev_offset, 2 * (offset - prev_offset));
            }
            prev_offset = offset;
            index += 64;
        }
        res[size - 1] = result.substr(128 + 2 * prev_offset);
        // bool success
        return 0;
    }
};

struct LogEntry {
    Address address;
    DBytes data;
    std::vector<Bytes32> topics;

    LogEntry(const evmc::address& addr, const uint8_t* data, size_t data_size, const evmc::bytes32 topics[], size_t num_topics)
    : address(addr), data(data, data_size), topics(topics, topics + num_topics) {
    }
    /// Equal operator.
    bool operator==(const LogEntry& other) const noexcept
    {
        return address == other.address && data == other.data && topics == other.topics;
    }
    LogEntry(const json& j) {
        address = j["address"].get<std::string>();
        data = j["data"].get<std::string>().substr(2);
        for (auto& t : j["topics"]) {
            topics.push_back(t.get<std::string>().substr(2));
        }
    }
    std::string to_string() const;
};

namespace std
{
/// Hash operator template specialization for evmc::address. Needed for unordered containers.
template <>
struct hash<Address>
{
    /// Hash operator using FNV1a-based folding.
    constexpr size_t operator()(const Address& s) const noexcept
    {
        hash<evmc::address> obj;
        return obj(s.value);
    }
};

template <>
struct hash<Bytes32>
{
    /// Hash operator using FNV1a-based folding.
    constexpr size_t operator()(const Bytes32& s) const noexcept
    {
        hash<evmc::bytes32> obj;
        return obj(s.value);
    }
};

}