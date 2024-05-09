#include "data/ipc.h"
#include <sys/socket.h>
#include <sys/un.h>

int IpcClient::connect(const std::string &address) {
    _client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_client_fd <= 0) {
        LOG(ERROR) << "create socket failed:" << _client_fd;
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, address.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(_client_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG(ERROR) << "connect unix socket failed:" << address;
        return -1;
    }
    return 0;
}

int IpcClient::read(json &out) {
    std::string message;
    char buffer[4096];
    ssize_t n;
    while ((n = ::recv(_client_fd, buffer, sizeof(buffer), 0)) > 0) {
        message.append(buffer, n);
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG(ERROR) << "read failed";
        return -1;
    }
    if (!message.empty()) {
        try {
            out = json::parse(message);
        } catch (const json::parse_error& e) {
            LOG(ERROR) << "JSON parse error: " << e.what();
            return -1;
        }
    }
    return 0;
}

int IpcClient::_write(const json& j) {
    std::string str = j.dump();
    if (send(_client_fd, str.c_str(), str.size(), 0) < 0) {
        LOG(ERROR) << "write failed";
        return -1;
    }
    return 0;
}