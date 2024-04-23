#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <string_view>
std::string HashAndTakeFirstFourBytes(const std::string& input);
std::string HashAndTakeAllBytes(const std::string& input);
std::string HashAndTakeAllBytes(const std::basic_string<uint8_t>& input);
std::string HashAndTakeAllBytes(const std::basic_string_view<uint8_t>& input);
std::string HashAndTakeAllBytes(const std::vector<uint8_t>& input);
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
        if (hexStr.length() != 64) {
            throw std::runtime_error("Invalid input length for decoding.");
        }

        ValueType result;
        std::stringstream ss;
        ss << std::hex << hexStr;
        ss >> result;
        return result;
    }
};

class Address : public SolidityType {
private:
    // 包含0x
    std::string value;

public:
    Address(const std::string& val) : value(val) {}

    std::string encode() const override {
        // Solidity地址是20字节，需要在左侧填充0至32字节
        std::string paddedValue = std::string(24, '0') + value.substr(2); // 移除"0x"前缀
        std::transform(paddedValue.begin(), paddedValue.end(), paddedValue.begin(), ::tolower);
        return paddedValue;
    }
    // 包含前面的0x
    static std::string decode(const std::string& encodedAddress) {
        if (encodedAddress.length() != 64) {
            throw std::runtime_error("Invalid encoded address length. Expected 64 hex characters (32 bytes).");
        }

        // 地址数据在ABI编码的填充中占据后20字节，所以跳过前12字节(24个十六进制字符)
        return "0x" + encodedAddress.substr(24, 40);
    }
};

template <size_t BitSize>
class Bytes {
public:
    using ValueType = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<BitSize, BitSize, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;

private:
    ValueType value;

public:
    Bytes(ValueType val) : value(val) {}

    std::string encode() const {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(64) << std::hex << value;
        return stream.str();
    }

    static ValueType decode(const std::string& hexStr) {
        if (hexStr.length() != 64) {
            throw std::runtime_error("Invalid input length for decoding.");
        }

        ValueType result;
        std::stringstream ss;
        ss << std::hex << hexStr;
        ss >> result;
        return result;
    }
};
// 动态Bytes
class DBytes : public SolidityType {
public:
    std::string value;
    DBytes(const std::string& val) : value(val) {}
    std::string encode() const override {
        // 注意只get了tail部分
        // value必须是已经编码的字符串
        size_t k = value.size();
        std::stringstream encoded;
        // 长度 原始的长度
        encoded << std::setfill('0') << std::setw(64) << std::hex << k / 2;
        encoded << value;
        // pad
        while (k % 64 != 0) {
            encoded << "0";
            ++k;
        }
        return encoded.str();
    }
    // 32bytes
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
            //LOG(INFO) << i;
            res[i] = result.substr(64 + i * 64, 64);
        }
        //LOG(INFO) << "32 " << res[0];
        return 0;
    }
};

class Call : public SolidityType {
public:
    Address target;
    DBytes callData;

    Call(const Address& target, const std::string& callData) : target(target), callData(callData) {}

    std::string encode() const {
        // 假设Address已经是正确的32字节字符串
        // callData应该也是正确编码的
        std::stringstream encoded;

        encoded << target.encode();
        encoded << std::setfill('0') << std::setw(64) << std::hex << 2 * 32;;// head of Data(offset)
        encoded << callData.encode();
        return encoded.str();
    }
    // @param *********** (no 0x)
    // @return bytes(data) 空则失败
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

    std::string encode() const override {
        // 开始构造数据
        std::stringstream encoded;
        
        // 首先是requireSuccess参数，作为bool类型编码，true为0x01，false为0x00，但需要填充到32字节
        encoded << std::setfill('0') << std::setw(64) << (requireSuccess ? 1 : 0);
        
        // 可变数组data的偏移位置
        encoded << std::setfill('0') << std::setw(64) << std::hex << 2 * 32;

        uint32_t call_size = calls.size();
        // 然后是calls数组长度，也需要编码到32字节
        encoded << std::setfill('0') << std::setw(64) << std::hex << call_size;

        std::stringstream tail;
        uint32_t len = 0; // 累计tail len 32字节为单位
        // 接下来是calls数组内容，每个Call都调用其encode方法
        for (const auto& call : calls) {
            std::string s = call.encode();
            if (s.size() % 64 != 0) {
                LOG(ERROR) << "Call encode size is not a multiple of 32.";
                break;
            }
            // 由于各个Call变长, 构造各个Head(Call), 即其encode的offset
            encoded << std::setfill('0') << std::setw(64) << std::hex << (len + call_size) * 32; // head
            len += s.size() / 64;
            tail << s; //tail
        }

        return encoded.str() + tail.str();
    }

    void clear() {
        calls.clear();
    }

    // 通过client获取结果, 所有调用必须成功
    // @param res: bytes data of each call unit
    int request_result(ClientBase* client, std::vector<std::string>& res, uint64_t block_num);
    // @return {bool(success), bytes(data)}
    // @param ***** (不含0x)
    static int decode(const std::string& result, std::vector<std::string>& res) {
        // 第一个位置是整个数组所在offset
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
        //LOG(INFO) << "size: " << size;
        res.resize(size);
        uint32_t prev_offset = 0;
        for (uint32_t j = 0; j < size; ++j) {
            encoded.clear();
            // head of Result[i] (offset)
            encoded << result.substr(index, 64);
            uint32_t offset = 0;
            encoded >> std::hex >> offset; 
            //LOG(INFO) << "offset: " << offset;
            if (prev_offset) {
                res[j-1] = result.substr(128 + 2 * prev_offset, 2 * (offset - prev_offset));
            }
            prev_offset = offset;
            index += 64;
        }
        //LOG(INFO) << "size : " << size << " prev_offset: " << prev_offset << "str.size" << result.size();
        res[size - 1] = result.substr(128 + 2 * prev_offset);
        //LOG(INFO) << "res[0]: " << res[0];
        // bool success
        return 0;
    }
};
