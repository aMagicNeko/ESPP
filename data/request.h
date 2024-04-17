#include "data/client.h"
#include "util/common.h"
#include "util/json_parser.h"

inline int request_balance(ClientBase* client, const std::string& addr_str, uint256_t& balence_data) {
    json json_data = {
        {"jsonrpc", "2.0"},
        {"method", "eth_getBalance"},
        {"params", {
            addr_str,
            "latest"
        }},
        {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "get balance failed:" << json_data.dump();
        return -1;
    }
    if (parse_json(json_data, "result", balence_data) != 0) {
        LOG(ERROR) << "get balance failed:" << json_data.dump();
        return -1;
    }
    return 0;
}

inline int request_code(ClientBase* client, const std::string& addr_str, std::string& code) {
    json json_data = {
        {"jsonrpc", "2.0"},
        {"method", "eth_getCode"},
        {"params", {
            addr_str,
            "latest"
        }},
        {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "get code failed:" << json_data.dump();
        return -1;
    }
    if (parse_json(json_data, "result", code) != 0) {
        LOG(ERROR) << "get code failed:" << json_data.dump();
        return -1;
    }
    return 0;
}

inline int request_storage(ClientBase* client, const std::string& addr_str, const std::string& key, std::string& result) {
    json json_data = {
        {"jsonrpc", "2.0"},
        {"method", "eth_getStorageAt"},
        {"params", {
            addr_str,
            key,
            "latest"
        }},
        {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "get storage failed:" << json_data.dump();
        return -1;
    }
    if (parse_json(json_data, "result", result) != 0) {
        LOG(ERROR) << "get storage failed:" << json_data.dump();
        return -1;
    }
    return 0;
}

inline int request_header_hash(ClientBase* client, uint64_t block_number, std::string& hash) {
    std::stringstream ss;
    ss << std::hex << block_number;
    std::string block;
    ss >> block;
    json json_data = {
        {"jsonrpc", "2.0"}, {"method", "eth_getHeaderByNumber"}, {"params", {"0x" + block}}, {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "get block by number failed" << json_data.dump();
        return -1;
    }
    if (json_data.find("result") == json_data.end()) {
        LOG(ERROR) << "get block by number failed" << json_data.dump();
        return -1;
    }
    if (parse_json(json_data["result"], "hash", hash) != 0) {
        LOG(ERROR) << "get block by number failed" << json_data.dump();
        return -1;
    }
    return 0;
}

inline int request_block_number(ClientBase* client, uint64_t& block_num) {
    json json_data = {
        {"jsonrpc", "2.0"},
        {"method", "eth_blockNumber"},
        {"params", {}},
        {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "get block number failed";
        return -1;
    }
    std::string tmp;
    if (parse_json(json_data, "result", tmp) != 0) {
        LOG(ERROR) << "get block number failed" << json_data.dump();
        return -1;
    }
    std::stringstream ss;
    ss << tmp.substr(2);
    ss >> std::hex >> block_num;
    return 0;
}

// 0 for latest
inline int request_txs_hash_by_number(ClientBase* client, uint64_t block_number, std::vector<std::string>& hashs) {
    json json_data = {
        {"jsonrpc", "2.0"}, {"method", "eth_getBlockByNumber"}, {"params", {"latest", false}}, {"id", 1}
    };
    if (block_number != 0) {
        std::stringstream ss;
        ss << std::hex << block_number;
        json_data["params"][0] = "0x" + ss.str();
    }
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "request_txs_hash_by_number failed";
        return -1;
    }
    if (json_data.find("result") == json_data.end()) {
        LOG(ERROR) << "request_txs_hash_by_number failed: " << json_data.dump();
        return -1;
    }
    if (json_data["result"].find("transactions") == json_data["result"].end()) {
        LOG(ERROR) << "request_txs_hash_by_number failed: " << json_data.dump();
        return -1;
    }
    for (std::string hash : json_data["result"]["transactions"]) {
        hashs.push_back(hash);
    }
    return 0;
}

// 0 for latest
// 1 for pending
inline int request_txs_by_number(ClientBase* client, uint64_t block_number, std::vector<json>& txs) {
    json json_data = {
        {"jsonrpc", "2.0"}, {"method", "eth_getBlockByNumber"}, {"params", {"latest", true}}, {"id", 1}
    };
    if (block_number == 1) {
        json_data["params"][0] = "pending";
    } else if (block_number != 0) {
        std::stringstream ss;
        ss << std::hex << block_number;
        json_data["params"][0] = "0x" + ss.str();
    }
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "request_txs_by_number failed";
        return -1;
    }
    if (json_data.find("result") == json_data.end()) {
        LOG(ERROR) << "request_txs_by_number failed: " << json_data.dump();
        return -1;
    }
    if (json_data["result"].find("transactions") == json_data["result"].end()) {
        LOG(ERROR) << "request_txs_by_number failed: " << json_data.dump();
        return -1;
    }
    for (auto tx : json_data["result"]["transactions"]) {
        txs.push_back(tx);
    }
    return 0;
}

inline int request_tx_receipt(ClientBase* client, std::string hash, json& res) {
    json json_data = {
        {"jsonrpc", "2.0"}, {"method", "eth_getTransactionReceipt"}, {"params", {hash}}, {"id", 1}
    };
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "request_tx_receipt failed";
        return -1;
    }
    if (json_data.find("result") == json_data.end()) {
        LOG(ERROR) << "request_tx_receipt failed: " << json_data.dump();
        return -1;
    }
    res = json_data["result"];
    return 0;
}

inline int request_nonce(ClientBase* client, std::string addr, uint64_t& nonce) {
    json json_data = {{"jsonrpc", "2.0"}, {"method", "eth_getTransactionCount"}, {"params", {addr, "latest"}}, {"id", 1}};
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "request_nonce failed";
        return -1;
    }
    if (parse_json(json_data, "result", nonce) != 0) {
        LOG(ERROR) << "request_nonce failed";
        return -1;
    }
    return 0;
}

inline int request_head_by_number(ClientBase* client, uint64_t block_number, uint64_t& timestamp, uint64_t& base_fee, uint64_t& gas_limit) {
    json json_data = {
        {"jsonrpc", "2.0"}, {"method", "eth_getBlockByNumber"}, {"params", {"latest", false}}, {"id", 1}
    };
    if (block_number == 1) {
        json_data["params"][0] = "pending";
    } else if (block_number != 0) {
        std::stringstream ss;
        ss << std::hex << block_number;
        json_data["params"][0] = "0x" + ss.str();
    }
    if (client->write_and_wait(json_data) != 0) {
        LOG(ERROR) << "request_head_by_number failed";
        return -1;
    }
    if (json_data.find("result") == json_data.end()) {
        LOG(ERROR) << "request_head_by_number failed: " << json_data.dump();
        return -1;
    }
    if (parse_json(json_data["result"], "timestamp", timestamp) != 0) {
        LOG(ERROR) << "get timestamp error" << json_data.dump();
    }
    if (parse_json(json_data["result"], "baseFeePerGas", base_fee) != 0) {
        LOG(ERROR) << "get base_fee error" << json_data.dump();
    }
    if (parse_json(json_data["result"], "gasLimit", gas_limit) != 0) {
        LOG(ERROR) << "get gas_limit error" << json_data.dump();
    }
    return 0;
}