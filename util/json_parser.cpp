#include "json_parser.h"

int hex_str_to_uint256(const std::string& hexStr, uint256_t& result) {
    result = 0;
    for (size_t i = 2; i < hexStr.size(); ++i) {
        char c = hexStr[i];
        result *= 16;
        if (c >= '0' && c <= '9') {
            result += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            result += 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            result += 10 + (c - 'A');
        } else {
            return -1;
        }
    }
    return 0;
}
