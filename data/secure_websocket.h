#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include "data/client.h"

namespace asio = boost::asio;
namespace beast = boost::beast;

class SecureWebsocket : public ClientBase {
public:
    SecureWebsocket() : _ctx(asio::ssl::context::sslv23_client), _tcp_socket(_io_context),
            _ssl_socket(_tcp_socket, _ctx), _ws(_ssl_socket) {}
    virtual ~SecureWebsocket() {}

    int connect(const std::string &host, const std::string &port, const std::string &path);
    
    int read(json &buffer) override;

    int _write(const json &in) override;
private:
    asio::io_context _io_context;
    asio::ssl::context _ctx;
    asio::ip::tcp::socket _tcp_socket;
    asio::ssl::stream<asio::ip::tcp::socket&> _ssl_socket;
    beast::websocket::stream<asio::ssl::stream<asio::ip::tcp::socket&>&> _ws;
    boost::system::error_code _ec;
};
