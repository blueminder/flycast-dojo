#include "DojoSession.hpp"

using asio::ip::tcp;

receiver_session::receiver_session(tcp::socket socket)
	: socket_(std::move(socket))
{
}

void receiver_session::start()
{
	dojo.receiver_header_read = false;
	dojo.receiver_start_read = false;
	do_read_header();
}

void receiver_session::do_read_header()
{
	auto self(shared_from_this());

	socket_.async_read_some(asio::buffer(data_, HEADER_LEN),
		[this, self](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				if (length == 0)
				{
					INFO_LOG(NETWORK, "Client disconnected");
					socket_.close();
				}

				if (length == HEADER_LEN)
				{
					unsigned int size = HeaderReader::GetSize((unsigned char*)data_);
					unsigned int seq = HeaderReader::GetSeq((unsigned char*)data_);
					unsigned int cmd = HeaderReader::GetCmd((unsigned char*)data_);

					working_size = size;
					working_cmd = cmd;

					memcpy(message, data_, HEADER_LEN);

					do_read_body();
				}

			}
		});
}

void receiver_session::do_read_body()
{
	auto self(shared_from_this());

	socket_.async_read_some(asio::buffer(&data_[HEADER_LEN], working_size),
		[this, self](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				if (working_size > 0 && length == working_size)
				{
					const char* body = data_ + HEADER_LEN;
					int offset = 0;

					dojo.ProcessBody(working_cmd, working_size, body, &offset);
				}

				do_read_header();
			}
		});
}

void receiver_session::do_write(std::size_t length)
{
	auto self(shared_from_this());
	asio::async_write(socket_, asio::buffer(data_, length),
		[this, self](std::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				INFO_LOG(NETWORK, "Message Sent: %s", data_);
				do_read_header();
			}
		});
}

AsyncTcpServer::AsyncTcpServer(asio::io_context& io_context, short port)
	: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
	do_accept();
}

void AsyncTcpServer::do_accept()
{
	acceptor_.async_accept(
		[this](std::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				std::make_shared<receiver_session>(std::move(socket))->start();
			}

			do_accept();
		});
}
