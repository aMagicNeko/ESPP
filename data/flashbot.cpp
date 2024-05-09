#include "data/flashbot.h"
#include "data/request.h"
const std::string DEFAULT_FLASHBOTS_RELAY = "https://relay.flashbots.net";
int FlashBotClient::init(ClientBase* client) {
    // Initialize the channel, specify it to use HTTPS
    brpc::ChannelOptions options;
    options.protocol = brpc::PROTOCOL_HTTP;
    options.connection_type = "single";
    options.timeout_ms = 1000 /*milliseconds*/;
    options.max_retry = 3;
    if (_channel.Init("DEFAULT_FLASHBOTS_RELAY", &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel" << std::endl;
        return -1;
    }
    _client = client;
    _id.store(0);
    return 0;
}

int FlashBotClient::send_bundles(const std::vector<std::string>& raw_txs, std::string& uid) {
    brpc::Controller cntl;
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    cntl.http_request().uri() = DEFAULT_FLASHBOTS_RELAY;
    cntl.http_request().set_content_type("application/json");
    // Send the request
    _channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    uint64_t block_number = _client->number();
    uint32_t id = _id.fetch_add(1);
    uid = "0x" + uint64_to_str(id);
    json json_data {
    {"jsonrpc", "2.0"},
    {"id", id},
    {"method", "eth_sendBundle"},
    {"params", {
        {
        {"txs", {}},
        {"blockNumber", "0x" + uint64_to_str(block_number)},
        {"replacementUuid", uid}
        }
    }
    }
    };
    for (const std::string& s:raw_txs) {
        json_data["params"][0]["txs"].push_back(s);
    }
    cntl.request_attachment().append(json_data.dump());
    if (cntl.Failed()) {
        LOG(ERROR) << "Fail to send HTTPS request: " << cntl.ErrorText() << std::endl;
        return -1;
    }
    LOG(INFO) << "Received response from server: " << cntl.response_attachment() << std::endl;
    return 0;
}
