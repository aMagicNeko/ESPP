#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>
using uint256_t = boost::multiprecision::uint256_t;
using uint128_t = boost::multiprecision::uint128_t;
using int256_t = boost::multiprecision::int256_t;
// https://github.com/Uniswap/v3-core/blob/main/contracts/libraries/TickMath.sol
uint256_t getSqrtRatioAtTick(int tick) {
    uint256_t sqrtPriceX96 = 0;
    uint256_t absTick = tick < 0 ? -tick : tick;
    if (absTick > 99999999999) {
        return 0;
    }
    uint256_t ratio = (absTick & 0x1) != 0 ? uint256_t("0xfffcb933bd6fad37aa2d162d1a594001") : uint256_t("0x100000000000000000000000000000000");
    if ((absTick & 0x2) != 0) ratio = (ratio * uint256_t("0xfff97272373d413259a46990580e213a")) >> 128;
    if ((absTick & 0x4) != 0) ratio = (ratio * uint256_t("0xfff2e50f5f656932ef12357cf3c7fdcc")) >> 128;
    if ((absTick & 0x8) != 0) ratio = (ratio * uint256_t("0xffe5caca7e10e4e61c3624eaa0941cd0")) >> 128;
    if ((absTick & 0x10) != 0) ratio = (ratio * uint256_t("0xffcb9843d60f6159c9db58835c926644")) >> 128;
    if ((absTick & 0x20) != 0) ratio = (ratio * uint256_t("0xff973b41fa98c081472e6896dfb254c0")) >> 128;
    if ((absTick & 0x40) != 0) ratio = (ratio * uint256_t("0xff2ea16466c96a3843ec78b326b52861")) >> 128;
    if ((absTick & 0x80) != 0) ratio = (ratio * uint256_t("0xfe5dee046a99a2a811c461f1969c3053")) >> 128;
    if ((absTick & 0x100) != 0) ratio = (ratio * uint256_t("0xfcbe86c7900a88aedcffc83b479aa3a4")) >> 128;
    if ((absTick & 0x200) != 0) ratio = (ratio * uint256_t("0xf987a7253ac413176f2b074cf7815e54")) >> 128;
    if ((absTick & 0x400) != 0) ratio = (ratio * uint256_t("0xf3392b0822b70005940c7a398e4b70f3")) >> 128;
    if ((absTick & 0x800) != 0) ratio = (ratio * uint256_t("0xe7159475a2c29b7443b29c7fa6e889d9")) >> 128;
    if ((absTick & 0x1000) != 0) ratio = (ratio * uint256_t("0xd097f3bdfd2022b8845ad8f792aa5825")) >> 128;
    if ((absTick & 0x2000) != 0) ratio = (ratio * uint256_t("0xa9f746462d870fdf8a65dc1f90e061e5")) >> 128;
    if ((absTick & 0x4000) != 0) ratio = (ratio * uint256_t("0x70d869a156d2a1b890bb3df62baf32f7")) >> 128;
    if ((absTick & 0x8000) != 0) ratio = (ratio * uint256_t("0x31be135f97d08fd981231505542fcfa6")) >> 128;
    if ((absTick & 0x10000) != 0) ratio = (ratio * uint256_t("0x9aa508b5b7a84e1c677de54f3e99bc9")) >> 128;
    if ((absTick & 0x20000) != 0) ratio = (ratio * uint256_t("0x5d6af8dedb81196699c329225ee604")) >> 128;
    if ((absTick & 0x40000) != 0) ratio = (ratio * uint256_t("0x2216e584f5fa1ea926041bedfe98")) >> 128;
    if ((absTick & 0x80000) != 0) ratio = (ratio * uint256_t("0x48a170391f7dc42444e8fa2")) >> 128;

    if (tick > 0) ratio = (std::numeric_limits<uint256_t>::max)() / ratio;

    // this divides by 1<<32 rounding up to go from a Q128.128 to a Q128.96.
    // we then downcast because we know the result always fits within 160 bits due to our tick input constraint
    // we round up in the division so getTickAtSqrtRatio of the output price is always consistent
    sqrtPriceX96 = ((ratio >> 32) + (ratio % (uint256_t(1) << 32) == 0 ? 0 : 1));
    return sqrtPriceX96;
}

std::string encode(int256_t value) {
    std::stringstream stream;
    if (value < 0) {
        uint256_t tmp = (-value).convert_to<uint256_t>();
        tmp = (std::numeric_limits<uint256_t>::max)() - tmp + 1;
        stream << std::setfill('f') << std::setw(64) << std::hex << tmp;
    } else {
        stream << std::setfill('0') << std::setw(64) << std::hex << value;
    }
    return stream.str();
}

class ByteData {
    std::vector<uint8_t> _data; // 数据存储

public:
    // 构造函数，用于初始化数据
    ByteData(const std::vector<uint8_t>& data) : _data(data) {}

    // 将字节转换为 uint256_t
    uint256_t to_uint256(size_t l, size_t r) const {
        uint256_t result = 0;
        for (size_t cur = l; cur < r; ++cur) {
            result <<= 8;
            result += _data[cur];
        }
        return result;
    }

    // 将字节转换为 int256_t
    int256_t to_int256(size_t l, size_t r) const {
        if (r <= l) {
            throw std::invalid_argument("Invalid range");
        }

        int256_t result = 0;
        bool isNegative = _data[l] & 0x80; // 检查最高位是否为1
        
        for (size_t cur = l; cur < r; ++cur) {
            result <<= 8;
            result += _data[cur];
        }

        if (isNegative) {
            // 如果是负数，我们需要计算补码
            int256_t mask = (int256_t(1) << ((r - l) * 8)) - 1;
            result = (result ^ mask) + 1;
            result = -result;
        }

        return result;
    }
};

// 示例：
int main() {
    std::vector<uint8_t> data = {0xFF, 0xEE, 0xDD, 0xCC};  // 例如，0xFFEEDDCC
    ByteData byteData(data);
    int256_t num = byteData.to_int256(0, 4);
    std::cout << "Converted number: " << num << std::endl;

    return 0;
}