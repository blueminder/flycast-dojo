#pragma once

#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

using asio::ip::tcp;

class async_tcp_client
{
public:
	async_tcp_client(asio::io_context& io_context, const tcp::resolver::results_type& endpoints);

	void start();

private:
	void do_connect(const tcp::resolver::results_type& endpoints);

	void do_read();
	void do_write();

	bool transmission_in_progress;

	asio::io_context& io_context_;
	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];

};
