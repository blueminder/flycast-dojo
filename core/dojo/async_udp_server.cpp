#include "DojoSession.hpp"

using asio::ip::udp;

async_udp_server::udp_server(asio::io_context& io_context, short port)
	: socket_(io_context, udp::endpoint(udp::v4(), port))
{
	do_receive();
}

void async_udp_server::do_receive()
{
	socket_.async_receive_from(
		asio::buffer(data_, max_length), sender_endpoint_,
		[this](std::error_code ec, std::size_t bytes_recvd)
	{
		if (!ec && bytes_recvd > 0)
		{

			if (memcmp("NAME", data_, 4) == 0)
			{
				settings.dojo.OpponentName = std::string(buffer + 5, strlen(buffer + 5));
				
				opponent_addr = sender;

				// prepare for delay selection
				if (dojo.OpponentIP.empty())
					dojo.OpponentIP = std::string(inet_ntoa(opponent_addr.sin_addr));

                deliver("OK NAME");

				dojo.isMatchReady = true;
				dojo.resume();
			}

			if (memcmp("OK NAME", data_, 7) == 0)
			{
				name_acknowledged = true;
			}

/*
			if (memcmp("PING", data_, 4) == 0)
			{
				char buffer_copy[256];
				memcpy(buffer_copy, data_, 256);
				buffer_copy[1] = 'O';

                std::string pong_response = std::string(buffer_copy, strlen(buffer_copy));
                deliver(pong_response);

				memset(data_, 0, 256);
			}

			if (memcmp("PONG", data_, 4) == 0)
			{
				int rnd_num_cmp = atoi(buffer + 5);
				long ret_timestamp = dojo.unix_timestamp();
				
				if (ping_send_ts.count(rnd_num_cmp) == 1)
				{

					long rtt = ret_timestamp - ping_send_ts[rnd_num_cmp];
					INFO_LOG(NETWORK, "Received PONG %d, RTT: %d ms", rnd_num_cmp, rtt);
					
					ping_rtt.push_back(rtt);

					if (ping_rtt.size() > 1)
					{
						avg_ping_ms = std::accumulate(ping_rtt.begin(), ping_rtt.end(), 0.0) / ping_rtt.size();
					}
					else
					{
						avg_ping_ms = rtt;
					}

					if (ping_rtt.size() > 5)
						ping_rtt.clear();

					ping_send_ts.erase(rnd_num_cmp);
				}
				
			}
*/

			if (memcmp("START", data_, 5) == 0)
			{
				int delay = atoi(buffer + 6);
				if (!dojo.session_started)
				{
					dojo.StartSession(delay);
				}
			}

			if (memcmp("DISCONNECT", data_, 10) == 0)
			{
                deliver("OK DISCONNECT");
				opponent_disconnected = true;
				dojo.disconnect_toggle = true;
			}

			if (memcmp("OK DISCONNECT", data_, 13) == 0)
			{
                deliver("OK DISCONNECT");
				dojo.disconnect_toggle = true;
			}

			if (bytes_recvd == dojo.PayloadSize())
			{
				if (!dojo.isMatchReady && dojo.GetPlayer((u8 *)buffer) == dojo.opponent)
				{
					//opponent_addr = sender;

					// prepare for delay selection
					//if (dojo.hosting)
						//dojo.OpponentIP = std::string(inet_ntoa(opponent_addr.sin_addr));

					dojo.isMatchReady = true;
					dojo.resume();
				}

				dojo.ClientReceiveAction((const char*)data_);
			}

			do_send();
		}
		else
		{
			do_receive();
		}
	});
}

void async_udp_server::do_send()
{
	send_in_progress = !msgs_to_send.empty()
	if(send_in_progress)
	{
		socket_.async_send_to(
			asio::buffer(msgs_to_send.front().data(),
				msgs_to_send.front().length()), sender_endpoint_,
			[this](std::error_code /*ec*/, std::size_t /*bytes_sent*/)
		{
			msgs_to_send.pop_front();
			do_send();
		});
	}

	do_receive();
}

void async_udp_server::deliver(std::string to_deliver)
{
    for (int i = 0; i < settings.dojo.PacketsPerFrame; i++)
	    msgs_to_send.push_back(to_deliver);

    INFO_LOG(NETWORK, "Message Sent: %s", to_deliver.data());
}

void async_udp_server::deliver_player_name()
{
    deliver("NAME " + settings.dojo.PlayerName);
}

void async_udp_server::start_session()
{
	std::stringstream start_ss("");
	start_ss << "START " << dojo.delay;
	std::string to_send_start = start_ss.str();

    deliver(to_send_start);

	INFO_LOG(NETWORK, "Session Started");
}

void async_udp_server::client_thread()
{	
	try
	{	
		asio::io_context io_context;

		udp_server s(io_context, settings.dojo.ServerPort);

		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}
