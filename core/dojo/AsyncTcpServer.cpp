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
					memcpy(message + HEADER_LEN, data_ + HEADER_LEN, working_size);
					const char* body = message + HEADER_LEN;

					if (working_cmd == SPECTATE_START)
						read_start_spectate();
					else if (working_cmd == GAME_BUFFER)
						read_frame();
				}

				do_read_header();
			}
		});
}

void receiver_session::read_start_spectate()
{
	auto self(shared_from_this());
	int offset = 0;

	const char* body = message + HEADER_LEN;
	
	unsigned int v = MessageReader::ReadInt(body, &offset);
	dojo.game_name = MessageReader::ReadString(body, &offset);
	std::string PlayerName = MessageReader::ReadString(body, &offset);
	std::string OpponentName = MessageReader::ReadString(body, &offset);
	std::string Quark = MessageReader::ReadString(body, &offset);
	std::string MatchCode = MessageReader::ReadString(body, &offset);

	config::PlayerName = PlayerName;
	config::OpponentName = OpponentName;
	config::Quark = Quark;

	dojo.receiver_start_read = true;
}

void receiver_session::read_frame()
{
	auto self(shared_from_this());
	int offset = 0;

	const char* body = message + HEADER_LEN;

	std::string frame = MessageReader::ReadString(body, &offset);

	//if (memcmp(frame.data(), { 0 }, FRAME_SIZE) == 0)
	if (memcmp(frame.data(), "000000000000", FRAME_SIZE) == 0)
	{
		dojo.receiver_ended = true;
	}
	else
	{
		dojo.AddNetFrame(frame.data());
		std::string added_frame_data = dojo.PrintFrameData("ADDED", (u8*)frame.data());

		std::cout << added_frame_data << std::endl;
		dojo.last_received_frame = dojo.GetEffectiveFrameNumber((u8*)frame.data());

		// buffer stream
		if (dojo.net_inputs[1].size() == config::RxFrameBuffer.get() &&
			dojo.FrameNumber < dojo.last_consecutive_common_frame)
			dojo.resume();
	}

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
