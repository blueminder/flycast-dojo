#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

using asio::ip::tcp;

class tcp_session
    : public std::enable_shared_from_this<tcp_session>
{
public:
    tcp_session(tcp::socket socket);

    void start();

private:
    void do_read();
    void do_write(std::size_t length);

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class async_tcp_server
{
public:
    async_tcp_server(asio::io_context& io_context, short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};