#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <ethash/keccak.hpp>
#include "data/request.h"
// Utility function to convert hex string to bytes
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Utility function to convert text to bytes
std::vector<uint8_t> text_to_bytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

// Utility function to convert integer to bytes
std::vector<uint8_t> int_to_bytes(int value) {
    std::vector<uint8_t> bytes;
    while (value > 0) {
        bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
    std::reverse(bytes.begin(), bytes.end());
    return bytes;
}

// Structure to hold the signable message
struct SignableMessage {
    std::vector<uint8_t> version;
    std::vector<uint8_t> header;
    std::vector<uint8_t> body;
};

SignableMessage encode_defunct(
    const std::vector<uint8_t>& primitive = {}, const std::string& hexstr = "", const std::string& text = "") {

    std::vector<uint8_t> message_bytes;

    if (!primitive.empty()) {
        message_bytes = primitive;
    } else if (!hexstr.empty()) {
        message_bytes = hex_to_bytes(hexstr);
    } else if (!text.empty()) {
        message_bytes = text_to_bytes(text);
    } else {
        throw std::invalid_argument("Must supply exactly one of primitive, hexstr, or text.");
    }

    std::string msg_length_str = std::to_string(message_bytes.size());
    std::vector<uint8_t> msg_length(msg_length_str.begin(), msg_length_str.end());

    // Combine header parts
    std::vector<uint8_t> header = {'t', 'h', 'e', 'r', 'e', 'u', 'm', ' ', 'S', 'i', 'g', 'n', 'e', 'd', ' ', 'M', 'e', 's', 's', 'a', 'g', 'e', ':', '\n'};
    header.insert(header.end(), msg_length.begin(), msg_length.end());

    // Create SignableMessage
    SignableMessage signable_message{
        {'E'}, // version
        header,
        message_bytes
    };

    return signable_message;
}

std::vector<uint8_t> hash_request_data(const SignableMessage& signable_message) {
    const auto& version = signable_message.version;

    std::vector<uint8_t> joined;
    joined.push_back(0x19);
    joined.insert(joined.end(), version.begin(), version.end());
    joined.insert(joined.end(), signable_message.header.begin(), signable_message.header.end());
    joined.insert(joined.end(), signable_message.body.begin(), signable_message.body.end());

    ethash::hash256 hash = ethash::keccak256(reinterpret_cast<const uint8_t*>(joined.data()), joined.size());
    return std::vector<uint8_t>(std::begin(hash.bytes), std::end(hash.bytes));
}

inline int sign_data(ClientBase* client, const std::string& address, const std::string& message, std::string& sig) {
    if (request_sign_data(client, address, message, sig) != 0) {
        return -1;
    }
    return 0;
}