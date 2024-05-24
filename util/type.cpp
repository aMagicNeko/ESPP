#include <cryptopp/keccak.h>
#include <cryptopp/hex.h>
#include <algorithm>
#include "data/request.h"
#include "util/type.h"
static const std::string multicall_address = "0xcA11bde05977b3631167028862bE2a173976CA11";
static const std::string muticall_head = HashAndTakeFirstFourBytes("tryAggregate(bool,(address,bytes)[])");

// return without 0x
std::string HashAndTakeFirstFourBytes(const std::string& input) {
    CryptoPP::Keccak_256 hash;
    std::string digest;
    
    hash.Update(reinterpret_cast<const CryptoPP::byte*>(input.data()), input.size());
    digest.resize(hash.DigestSize());
    hash.Final(reinterpret_cast<CryptoPP::byte*>(&digest[0]));
    
    std::string hexDigest;
    CryptoPP::HexEncoder encoder;
    encoder.Put(reinterpret_cast<const CryptoPP::byte*>(digest.data()), 4);
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
    CryptoPP::Keccak_256 hash;
    std::string digest;
    
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

// no 0x
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

Address::Address(const Bytes32& b) {
    memcpy(&value.bytes[0], b.value.bytes + 12, sizeof(value.bytes));
}

Address::Address(const DBytes& db, uint32_t l, uint32_t r) {
    memcpy(&value.bytes[0], &db._data[l], sizeof(value.bytes));
}

// Helper function to convert a single hex character to a byte
std::string LogEntry::to_string() const {
    std::ostringstream oss;
    oss << "Address: " << address.to_string() << '\n';
    oss << "Data: " << data.to_string() << '\n';
    oss << "Topics: ";
    for(auto topic : topics) {
        oss << topic.to_string() << ' ';
    }
    oss << '\n';
    return oss.str();
}