#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

using asio::ip::tcp;

class receiver_session
    : public std::enable_shared_from_this<receiver_session>
{
public:
    receiver_session(tcp::socket socket);

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
    async_tcp_server();
    async_tcp_server(asio::io_context& io_context, short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};

class transmitter_session
    : public std::enable_shared_from_this<transmitter_session>
{
public:
    transmitter_session(tcp::socket socket);

    void start();

private:
    void do_connect(const tcp::resolver::results_type& endpoints);

    void do_read();
    void do_write();

    bool transmission_in_progress;

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};

class async_tcp_client
{
public:
    async_tcp_client(asio::io_context& io_context, const tcp::resolver::results_type& endpoints);

private:
    void do_connect(const tcp::resolver::results_type& endpoints);

    asio::io_context& io_context_;
    tcp::socket socket_;
};
