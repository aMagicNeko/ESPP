#include <cryptopp/keccak.h>
#include <cryptopp/hex.h>
#include <algorithm>
#include "data/request.h"
static int check_byte_order() {
    int x = 1;
    char *p = (char *)&x;
    if (*p == 1) {
        return 0; // little endian
    }
    else {
        return 1; // big endian
    }
}
static const std::string multicall_address = "0xcA11bde05977b3631167028862bE2a173976CA11";
static const std::string muticall_head = HashAndTakeFirstFourBytes("tryAggregate(bool,(address,bytes)[])");

static int s_byte_order = check_byte_order();

// 不含0x
std::string HashAndTakeFirstFourBytes(const std::string& input) {
    // 创建Keccak-256哈希对象
    CryptoPP::Keccak_256 hash;
    std::string digest;
    
    // 计算哈希值
    hash.Update(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
    digest.resize(hash.DigestSize());
    hash.Final(reinterpret_cast<CryptoPP::byte*>(&digest[0]));
    
    // 将前4个字节转换为十六进制字符串
    std::string hexDigest;
    CryptoPP::HexEncoder encoder;
    encoder.Put(reinterpret_cast<const CryptoPP::byte*>(digest.data()), 4); // 只取前4个字节
    encoder.MessageEnd();
    
    CryptoPP::word64 size = encoder.MaxRetrievable();
    if(size) {
        hexDigest.resize(size);
        encoder.Get(reinterpret_cast<CryptoPP::byte*>(&hexDigest[0]), hexDigest.size());
    }
    std::transform(hexDigest.begin(), hexDigest.end(), hexDigest.begin(), [](unsigned char c) { 
        return std::tolower(c); 
    });
    return hexDigest;
}

std::string HashAndTakeAllBytes(const unsigned char* data, size_t data_size) {
    // 创建Keccak-256哈希对象
    CryptoPP::Keccak_256 hash;
    std::string digest;
    
    // 计算哈希值
    hash.Update(reinterpret_cast<const CryptoPP::byte*>(data), data_size);
    digest.resize(hash.DigestSize());
    hash.Final(reinterpret_cast<CryptoPP::byte*>(&digest[0]));
    
    std::string hexDigest;
    CryptoPP::HexEncoder encoder;
    encoder.Put(reinterpret_cast<const CryptoPP::byte*>(digest.data()), digest.size());
    encoder.MessageEnd();
    
    CryptoPP::word64 size = encoder.MaxRetrievable();
    if(size) {
        hexDigest.resize(size);
        encoder.Get(reinterpret_cast<CryptoPP::byte*>(&hexDigest[0]), hexDigest.size());
    }
    
    // Convert all characters to lowercase
    std::transform(hexDigest.begin(), hexDigest.end(), hexDigest.begin(), [](unsigned char c) { 
        return std::tolower(c); 
    });
    
    return hexDigest;
}

// 不含0x
std::string HashAndTakeAllBytes(const std::string& input) {
    return HashAndTakeAllBytes(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
}

std::string HashAndTakeAllBytes(const std::basic_string<uint8_t>& input) {
    return HashAndTakeAllBytes(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
}

std::string HashAndTakeAllBytes(const std::basic_string_view<uint8_t>& input) {
    return HashAndTakeAllBytes(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
}

std::string HashAndTakeAllBytes(const std::vector<uint8_t>& input) {
    return HashAndTakeAllBytes(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
}

int MultiCall::request_result(ClientBase* client, std::vector<std::string>& res, uint64_t block_num) {
    std::string method = "0x" + muticall_head + encode();
    std::string data;
    if (request_call(client, multicall_address, method, data, block_num) != 0) {
        return -1;
    }
    std::vector<std::string> ress;
    if (MultiCall::decode(data.substr(2), ress) != 0) {
        LOG(ERROR) << "decode multicall failed";
        return -1;
    }
    for (uint k = 0; k < ress.size(); ++k) {
        std::string tmp;
        if (Call::decode(ress[k], tmp) != 0) {
            LOG(ERROR) << "call failed";
            return -1;
        }
        res.push_back(tmp);
    }
    return 0;
}