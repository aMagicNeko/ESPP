#pragma once
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "data/client.h"

namespace asio = boost::asio;
namespace beast = boost::beast;

class Websocket : public ClientBase {
public:
    Websocket() : _ws(_io_context) {}

    int connect(const std::string &host, const std::string &port, const std::string &path);
    
    int read(json &buffer) override;

    int _write(const json &in) override;
private:
    asio::io_context _io_context;
    beast::websocket::stream<asio::ip::tcp::socket> _ws;
    boost::system::error_code _ec;
};