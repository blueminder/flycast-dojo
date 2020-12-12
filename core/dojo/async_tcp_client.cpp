#include "DojoSession.hpp"

using asio::ip::tcp;

void async_tcp_client::start()
{
	do_read();
}

void async_tcp_client::do_write()
{
	transmission_in_progress = !dojo.transmission_frames.empty();
	if (transmission_in_progress)
	{
		asio::async_write(socket_, asio::buffer(dojo.transmission_frames.front().data(), dojo.transmission_frames.front().length()),
			[this](std::error_code ec, std::size_t /*length*/)
			{
				if (!ec)
				{
					do_read();
				}
			});
	}
}

void async_tcp_client::do_read()
{
	socket_.async_read_some(asio::buffer(data_, max_length),
		[this](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				if (length == 0)
				{
					INFO_LOG(NETWORK, "spectator disconnected");
					socket_.close();
				}

				if (length >= 12)
				{

					std::string frame = std::string(data_, max_length);
					//INFO_LOG(NETWORK, "SPECTATED %s", frame_s.data());
					INFO_LOG(NETWORK, "FRAME %d", (int)dojo.FrameNumber);
					dojo.AddNetFrame(frame.data());
					dojo.PrintFrameData("SPECTATED", (u8*)frame.data());
				}

				do_write();
			}
		});
}

void async_tcp_client::do_connect(const tcp::resolver::results_type& endpoints)
{
	asio::async_connect(socket_, endpoints,
		[this](std::error_code ec, tcp::endpoint)
		{
			if (!ec)
			{
				do_read();
			}
		});
}

async_tcp_client::async_tcp_client(asio::io_context& io_context,
	const tcp::resolver::results_type& endpoints)
	: io_context_(io_context), socket_(io_context)
{
	do_connect(endpoints);
}