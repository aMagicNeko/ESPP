#include "data/websocket.h"
int Websocket::connect(const std::string &host, const std::string &port, const std::string &path) {
    try {
        auto const results = asio::ip::tcp::resolver(_io_context).resolve(host, port);

        asio::connect(_ws.next_layer(), results.begin(), results.end());

        _ws.handshake(host, path);
    }
    catch (std::exception& e) {
        LOG(FATAL) << "Connection failed: " << e.what();
        abort();
    }
    int fd = _ws.next_layer().native_handle();
    if (fd < 0) {
        LOG(ERROR) << "websocket get fd error";
        return -1;
    }
    LOG(INFO) << "tcp_socket: " << fd;
    set_fd(fd);
    return 0;
}

int Websocket::read(json &in) {
    beast::flat_buffer buffer;
    _ws.read(buffer, _ec);
    if (_ec) {
        LOG(ERROR) << "websocket read error: " << _ec.message().c_str();
        return -1;
    }
    std::string message = beast::buffers_to_string(buffer.data());
    try {
        in = json::parse(message);
    } catch (json::parse_error& e) {
        LOG(ERROR) << "JSON parse error: %s" << e.what();
        return -1;
    }
    return 0;
}

int Websocket::_write(const json &in) {
    std::string message = in.dump();
    _ws.write(boost::asio::buffer(message), _ec);
    if (_ec) {
        LOG(ERROR) << "websocket write [message: "<< message.c_str() << "] error: " << _ec.message().c_str();
        return -1;
    }
    return 0;
}