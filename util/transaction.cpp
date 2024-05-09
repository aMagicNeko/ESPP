#include "util/transaction.h"
#include "util/Aleth/TransactionBase.h"
dev::u160 convertAddress(const Address& addr) {
    dev::u160 ret = 0;
    for (size_t i = 0; i < 20; ++i) {
        ret <<= 8;
        ret |= addr.value.bytes[i];
    }
    return ret;
}

inline dev::Secret convertKey(const std::string& secret_key) {
    assert(secret_key.size() == 64);
    dev::Secret ret = dev::Secret(dev::fromHex(secret_key));
    return ret;
}

std::string Transaction::get_raw_tx(const std::string& secret_key, uint256_t base_fee) {
    auto key = convertKey(secret_key);
    assert(toAddress(key) == convertAddress(from));
    dev::eth::TransactionBase tx(value, priority_fee + base_fee, gas, input._data, nonce, key);
    std::string raw_tx = dev::toHex(tx.rlp());
    LOG(INFO) << "raw_tx:" << raw_tx;
    return raw_tx;
}