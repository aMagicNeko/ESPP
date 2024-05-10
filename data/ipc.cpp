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
    char ch;
    std::string line;
    while (true) {
        ssize_t n = recv(_client_fd, &ch, 1, 0);
        if (n > 0) {
            if (ch == '\n') {
                break;
            }
            line += ch;
        } else if (n == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                LOG(ERROR) << "recv error";
            }
        }
    }
    if (!line.empty()) {
        try {
            out = json::parse(line);
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