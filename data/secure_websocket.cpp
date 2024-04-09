#include "data/secure_websocket.h"
DEFINE_string(verify_file, "/etc/pki/tls/certs/ca-bundle.crt", "CA file path");

int SecureWebsocket::connect(const std::string &host, const std::string &port, const std::string &path) {
    try {
        // 设置 SSL 上下文
        _ctx.set_default_verify_paths(); // 使用系统默认的CA证书路径
        _ctx.set_verify_mode(ssl::verify_peer); // 开启对等方证书的验证
        // 连接到服务器
        asio::ip::tcp::resolver resolver(_io_context);
        auto const results = resolver.resolve(host, port);
        asio::connect(_tcp_socket, results.begin(), results.end());
        // 设置 SNI 主机名
        if (!SSL_set_tlsext_host_name(_ssl_socket.native_handle(), host.c_str())) {
            LOG(FATAL) << "set tlxset failed";
        }
        // SSL handshake
        _ssl_socket.handshake(asio::ssl::stream_base::client);

        // WebSocket handshake
        _ws.handshake(host, path);
    }
    catch (const std::exception& e) {
        LOG(FATAL) << "Connection failed: " << e.what();
        abort();
    }
    int fd = _tcp_socket.native_handle();
    if (fd < 0) {
        LOG(FATAL) << "tcp_socket get fd failed";
        return -1;
    }
    set_fd(fd);
    return 0;
}

int SecureWebsocket::read(json &in) {
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

int SecureWebsocket::_write(const json &in) {
    std::string message = in.dump();
    _ws.write(boost::asio::buffer(message), _ec);
    if (_ec) {
        LOG(ERROR) << "websocket write [message: "<< message.c_str() << "] error: " << _ec.message().c_str();
        return -1;
    }
    return 0;
}