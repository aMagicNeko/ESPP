#pragma once
#include <simulate/evmc.hpp>
#include <util/common.h>
namespace evmc {
inline uint8_t hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument("Invalid hex character");
}

inline std::string evmc_address_to_str(const evmc_address& addr) {
    std::string ret = "0x0000000000000000000000000000000000000000";

    const char hex_digits[] = "0123456789abcdef";

    // ret.data() 返回指向字符数组的指针
    char* out = &ret[2]; // 跳过"0x"

    for (int i = 0; i < 20; i++) {
        // 获取当前字节
        unsigned char byte = addr.bytes[i];

        // 高四位转为十六进制字符
        *out++ = hex_digits[byte >> 4];

        // 低四位转为十六进制字符
        *out++ = hex_digits[byte & 0x0F];
    }
    return ret;
}

// Convert a hex string to an evmc::address
inline address str_to_address(const std::string& str) {
    if (str.size() != 40 && str.size() != 42) {
        throw std::invalid_argument("Invalid address length");
    }

    size_t start_idx = (str.size() == 42 && str[0] == '0' && str[1] == 'x') ? 2 : 0;

    if (str.size() - start_idx != 40) {
        throw std::invalid_argument("Invalid address length after prefix");
    }

    evmc::address address;
    for (size_t i = 0; i < 20; ++i) {
        address.bytes[i] = (hex_char_to_byte(str[start_idx + 2 * i]) << 4)
                          + hex_char_to_byte(str[start_idx + 2 * i + 1]);
    }

    return address;
}

inline bytes32 uint256_to_bytes32(const uint256_t& value) {
    bytes32 result;
    for (size_t i = 0; i < sizeof(result.bytes) / sizeof(result.bytes[0]); ++i) {
        result.bytes[sizeof(result.bytes) - 1 - i] = static_cast<uint8_t>(value >> (i * 8));
    }
    return result;
}

inline bytes32 str_to_bytes32(const std::string& s) {
    assert(s.size() == 64);  // Must be exactly 64 hex characters (32 bytes)

    bytes32 bytes(0);
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned char high = hex_char_to_byte(s[i]);
        unsigned char low = hex_char_to_byte(s[i + 1]);
        bytes.bytes[i / 2] = (high << 4) | low;
    }
    return bytes;
}

inline std::string bytes32_to_str(const evmc_bytes32& bytes) {
    std::string result = "0x";
    result.reserve(66); // 2 characters for "0x" + 64 characters for hex

    const char hex_digits[] = "0123456789abcdef";

    for (int i = 0; i < 32; ++i) {
        unsigned char byte = bytes.bytes[i];
        result.push_back(hex_digits[byte >> 4]);   // High nibble
        result.push_back(hex_digits[byte & 0x0F]); // Low nibble
    }
    return result;
}

inline bytes32 add_bytes32(const evmc_bytes32& a, const evmc_bytes32& b) {
    bytes32 result = {};
    bool carry = false;
    
    for (int i = 31; i >= 0; --i) {
        uint16_t byte_sum = static_cast<uint16_t>(a.bytes[i]) + static_cast<uint16_t>(b.bytes[i]) + carry;
        result.bytes[i] = static_cast<uint8_t>(byte_sum & 0xff);
        carry = byte_sum > 0xff;
    }
    
    // 注意: 如果最高位产生进位则会溢出，但在256位整数中这是可接受的
    return result;
}

inline bytes32 sub_bytes32(const evmc_bytes32& a, const evmc_bytes32& b) {
    bytes32 result = {};
    bool borrow = false;

    for (int i = 31; i >= 0; --i) {
        uint16_t temp = static_cast<uint16_t>(a.bytes[i]) - static_cast<uint16_t>(b.bytes[i]) - borrow;
        result.bytes[i] = static_cast<uint8_t>(temp & 0xff);
        borrow = a.bytes[i] < b.bytes[i] + borrow;
    }

    // 注意: 如果最高位产生借位则会出现负数，但在256位整数中这是可接受的
    return result;
}

// Helper function to convert byte data to hex string
inline std::string to_hex_string(const uint8_t* data, size_t size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i)
        ss << std::setw(2) << static_cast<int>(data[i]);
    return ss.str();
}

// Convert evmc_message to string
inline std::string evmc_message_to_string(const evmc_message& msg) {
    std::stringstream ss;
    ss << "EVMC Message:" << std::endl;
    ss << "  Kind: " << static_cast<int>(msg.kind) << std::endl;
    ss << "  Flags: " << msg.flags << std::endl;
    ss << "  Depth: " << msg.depth << std::endl;
    ss << "  Gas: " << msg.gas << std::endl;
    ss << "  Recipient: " << evmc_address_to_str(msg.recipient) << std::endl;
    ss << "  Sender: " << evmc_address_to_str(msg.sender) << std::endl;
    ss << "  Input Data: " << (msg.input_data ? to_hex_string(msg.input_data, msg.input_size) : "NULL") << std::endl;
    ss << "  Value: " << bytes32_to_str(msg.value) << std::endl;
    ss << "  Create2 Salt: " << bytes32_to_str(msg.create2_salt) << std::endl;
    ss << "  Code Address: " << evmc_address_to_str(msg.code_address) << std::endl;
    // not initialized in evmone
    //ss << "  Code: " << (msg.code ? to_hex_string(msg.code, msg.code_size) : "NULL") << std::endl;
    return ss.str();
}

inline void evmc_address_copy(evmc_address* dest, const evmc_address* src) {
    memcpy(dest->bytes, src->bytes, sizeof(src->bytes));
}

inline void evmc_bytes32_copy(evmc_bytes32* dest, const evmc_bytes32* src) {
    memcpy(dest->bytes, src->bytes, sizeof(src->bytes));
}
}