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
	void do_read_header();
	void do_read_frame();
	void do_write(std::size_t length);

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
};

class AsyncTcpServer
{
public:
	AsyncTcpServer();
	AsyncTcpServer(asio::io_context& io_context, short port);

private:
	void do_accept();

	tcp::acceptor acceptor_;
};

