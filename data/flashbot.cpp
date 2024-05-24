#include "data/flashbot.h"
#include "data/request.h"
#include <ethash/keccak.hpp>
#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <cryptopp/keccak.h>
#include <cryptopp/ecp.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/hex.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// 将字符串转换为十六进制表示
std::string stringToHex(const std::string& input) {
    const char* hex_chars = "0123456789abcdef";
    std::string output;
    output.reserve(input.size() * 2 + 2); // 预分配空间以提升性能
    output.append("0x"); // 以太坊十六进制字符串前缀

    for (unsigned char c : input) {
        output.push_back(hex_chars[c >> 4]); // 高四位
        output.push_back(hex_chars[c & 0x0F]); // 低四位
    }

    return output;
}

std::string generateUUID() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return to_string(uuid);
}

std::vector<uint8_t> sign_message(const std::vector<uint8_t>& hash, const std::vector<uint8_t>& private_key) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    secp256k1_ecdsa_recoverable_signature signature;
    secp256k1_ecdsa_sign_recoverable(ctx, &signature, hash.data(), private_key.data(), nullptr, nullptr);

    std::vector<uint8_t> sig(65);  // 65 bytes for the signature including the recovery id
    int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, sig.data(), &recid, &signature);
    
    sig[64] = recid + 27;  // Add recovery id to the signature

    secp256k1_context_destroy(ctx);

    return sig;
}

const std::string address = "0xa22cf23D58977639e05B45A3Db7dF6D4a91eb892";

std::string get_sign_head(const std::string& message) {
    std::string sig;
    ethash::hash256 hash = ethash::keccak256(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    DBytes data(hash.bytes, 32);
    std::string tmp_message = data.to_string();
    tmp_message = std::string("\x19") + "Ethereum Signed Message:\n" + std::to_string(tmp_message.size()) + tmp_message;
    hash = ethash::keccak256(reinterpret_cast<const uint8_t*>(tmp_message.data()), tmp_message.size());
    data = DBytes(hash.bytes, 32);
    DBytes private_key("979a2f5e87e873629eabd0e3d4f82020d279375c8b2ae588fff6abba98e1f192");
    auto sign = sign_message(data._data, private_key._data);
    sig = DBytes(sign.data(), sign.size()).to_string();
    return address + ":" + sig;
}
const DBytes private_key = DBytes("979a2f5e87e873629eabd0e3d4f82020d279375c8b2ae588fff6abba98e1f192");

int FlashBotClient::init(ClientBase* client, std::string relay_url) {
    // Initialize the channel, specify it to use HTTPS
    bthread_mutex_init(&_mutex, 0);
    _relay_url = relay_url;
    brpc::ChannelOptions options;
    //options.mutable_ssl_options();
    options.protocol = brpc::PROTOCOL_HTTP;
    options.connection_type = "pooled";
    options.timeout_ms = 1000 /*milliseconds*/;
    options.max_retry = 10;
    if (_channel.Init(relay_url.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel:" << _relay_url;
        return -1;
    }
    _client = client;
    _id.store(0);
    return 0;
}

void FlashBotClient::on_head() {
    LockGuard lock(&_mutex);
    _uid.clear();
}

int FlashBotClient::send_bundles(const std::vector<std::string>& raw_txs) {
    uint64_t block_number = _client->number();
    uint32_t id = _id.fetch_add(1);
    if (_uid.size() == 0) {
        _uid = generateUUID();
    }
    
    json json_data {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "eth_sendBundle"},
        {"params", {
                {
                    {"txs", {}},
                    {"blockNumber", "0x" + uint64_to_str(block_number)},
                    {"replacementUuid", _uid}
                }
            }
        }
    };
    /*
    
    json json_data {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "eth_callBundle"},
        {"params", {
                {
                    {"txs", {}},
                    {"blockNumber", "0x" + uint64_to_str(block_number)},
                    {"stateBlockNumber", "latest"}
                }
            }
        }
    };
    LOG(INFO) << "start to send bundle:" << json_data.dump();
    */
    for (const std::string& s:raw_txs) {
        json_data["params"][0]["txs"].push_back(s);
    }
    std::string flashbots_signature = get_sign_head(json_data.dump());
    brpc::Controller cntl;
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    cntl.http_request().uri() = _relay_url;
    cntl.http_request().set_content_type("application/json");
    cntl.http_request().SetHeader("X-Flashbots-Signature", flashbots_signature);
    cntl.request_attachment().append(json_data.dump());

    LockGuard lock(&_mutex);
    // Send the request
    //LOG(INFO) << "data:" << json_data.dump();
    _channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
        std::string error_txt = cntl.ErrorText();
        LOG(ERROR) << "Fail to send HTTPS request: " << error_txt << " relay_url:" << _relay_url;
        return -1;
    }
    LOG(INFO) << "Received response from server: " << cntl.response_attachment() << " relay_url:" << _relay_url;
    
    if (_relay_url == "https://rpc.titanbuilder.xyz") {
        auto x = cntl.response_attachment().to_string();
        json response = json::parse(x);
        std::string hash = response["result"]["bundleHash"];
        brpc::Controller cntl1;
        cntl1.http_request().set_method(brpc::HTTP_METHOD_POST);
        cntl1.http_request().uri() = _relay_url;
        cntl1.http_request().set_content_type("application/json");
        json res_json = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "titan_getBundleStats"},
            {"params", 
                {
                    {
                        {"bundleHash", hash}
                    }
                }
            }
        };
        //LOG(INFO) << "titan_getBundleStats:" << res_json.dump();
        flashbots_signature = get_sign_head(res_json.dump());
        cntl1.http_request().SetHeader("X-Flashbots-Signature", flashbots_signature);
        cntl1.request_attachment().append(res_json.dump());
        _channel.CallMethod(NULL, &cntl1, NULL, NULL, NULL);
        if (cntl1.Failed()) {
            std::string error_txt = cntl1.ErrorText();
            LOG(ERROR) << "titan_getBundleStats failed: " << error_txt << " relay_url:" << _relay_url;
            return -1;
        }
        else {
            LOG(INFO) << "titan_getBundleStats:" << cntl1.response_attachment();
        }
    }
    else if (_relay_url == "https://relay.flashbots.net/") {
        auto x = cntl.response_attachment().to_string();
        json response = json::parse(x);
        std::string hash = response["result"]["bundleHash"];
        brpc::Controller cntl1;
        cntl1.http_request().set_method(brpc::HTTP_METHOD_POST);
        cntl1.http_request().uri() = _relay_url;
        cntl1.http_request().set_content_type("application/json");
        json res_json = {
            {"jsonrpc", "2.0"},
            {"id", 1},
            {"method", "flashbots_getBundleStats"},
            {"params", 
                {
                    {
                        {"bundleHash", hash},
                        {"blockNumber", "0x" + uint64_to_str(block_number)}
                    }
                }
            }
        };
        LOG(INFO) << "flashbots_getBundleStats:" << res_json.dump(); 
        flashbots_signature = get_sign_head(res_json.dump());
        cntl1.http_request().SetHeader("X-Flashbots-Signature", flashbots_signature);
        cntl1.request_attachment().append(res_json.dump());
        
        _channel.CallMethod(NULL, &cntl1, NULL, NULL, NULL);
        if (cntl1.Failed()) {
            std::string error_txt = cntl1.ErrorText();
            LOG(ERROR) << "flashbots_getBundleStats result failed: " << error_txt << " relay_url:" << _relay_url;
            return -1;
        }
        else {
            LOG(INFO) << "flashbots_getBundleStats:" << cntl1.response_attachment();
        }
    }
    
    return 0;
}