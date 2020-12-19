#include "DojoSession.hpp"

using asio::ip::tcp;

receiver_session::receiver_session(tcp::socket socket)
	: socket_(std::move(socket))
{
}

void receiver_session::start()
{
	do_read();
}

void receiver_session::do_read()
{
	auto self(shared_from_this());

	socket_.async_read_some(asio::buffer(data_, FRAME_SIZE),
		[this, self](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				if (length == 0)
				{
					INFO_LOG(NETWORK, "Client disconnected");
					socket_.close();
				}

				if (length == FRAME_SIZE)
				{
					std::string frame = std::string(data_, FRAME_SIZE);
					//if (memcmp(frame.data(), { 0 }, FRAME_SIZE) == 0)
					if (memcmp(frame.data(), "000000000000", FRAME_SIZE) == 0)
					{
						dojo.receiver_ended = true;
					}
					else
					{
						dojo.AddNetFrame(frame.data());
						dojo.PrintFrameData("ADDED", (u8*)frame.data());

						dojo.last_received_frame = dojo.GetEffectiveFrameNumber((u8*)frame.data());

						// buffer stream
						if (dojo.net_inputs[1].size() == 600 &&
							dojo.FrameNumber < dojo.last_consecutive_common_frame)
							dojo.resume();
					}
				}

				do_read();

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
				do_read();
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
