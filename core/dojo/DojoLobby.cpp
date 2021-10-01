#include "DojoSession.hpp"

void DojoLobby::BeaconThread()
{
	try
	{
		dojo.presence.multicast_port = std::stoul(config::LobbyMulticastPort);

		asio::io_context io_context;
		Beacon b(io_context,
			asio::ip::make_address(config::LobbyMulticastAddress));
		io_context.run();
	}
	catch (...) {}
}

void DojoLobby::ListenerThread()
{
	try
	{
		dojo.presence.multicast_port = std::stoul(config::LobbyMulticastPort);

		asio::io_context io_context;
		Listener l(io_context,
			asio::ip::make_address("0.0.0.0"),
			asio::ip::make_address(config::LobbyMulticastAddress));

		dojo.lobby_active = true;

		io_context.run();
	}
	catch (...) {}
}

std::string DojoLobby::ConstructMsg()
{
	std::string current_game = get_file_basename(settings.content.path);
	current_game = current_game.substr(current_game.find_last_of("/\\") + 1);

	std::string status;

	const char* message;

	switch (dojo.host_status)
	{
	case 0:
		status = "Idle";
		break;
	case 1:
		status = "Hosting, Waiting";
		break;
	case 2:
		status = "Hosting, Starting";
		break;
	case 3:
		status = "Hosting, Playing";
		break;
	case 4:
		status = "Guest, Connecting";
		break;
	case 5:
		status = "Guest, Playing";
		break;
	}

	std::stringstream message_ss("");
	message_ss << "2001_" << config::DojoServerPort.get() << "__" << status << "__" << current_game << "__" << config::PlayerName.get();
	if (dojo.host_status == 2 || dojo.host_status == 3)
	{
		message_ss << " vs " << config::OpponentName.get() << "__";
	}
	else
	{
		message_ss << "__";
	}
	message_ss << dojo.remaining_spectators << "__";
	std::string message_str = message_ss.str();

	return message_str;
}


void DojoLobby::ListenerAction(asio::ip::udp::endpoint beacon_endpoint, char* msgbuf, int length)
{
	if (memcmp(msgbuf, "2001_", strlen("2001_")) == 0)
	{
		// omit multicast packet unique identifier
		msgbuf = msgbuf + strlen("2001_");

		std::string ip_str = beacon_endpoint.address().to_string();
		std::string port_str = std::to_string(beacon_endpoint.port());

		std::stringstream bi;
		bi << ip_str << ":" << port_str;
		std::string beacon_id = bi.str();

		std::stringstream bm;
		bm << ip_str <<  "__" << msgbuf;

		if (dojo.host_status == 0)
		{
			if (active_beacons.count(beacon_id) == 0)
			{
				active_beacons.insert(std::pair<std::string, std::string>(beacon_id, bm.str()));

				uint64_t avg_ping_ms = dojo.client.GetAvgPing(beacon_id.c_str(), beacon_endpoint.port());
				active_beacon_ping.insert(std::pair<std::string, uint64_t>(beacon_id, avg_ping_ms));
			}
			else
			{
				try {
					if (active_beacons.at(beacon_id) != bm.str())
						active_beacons.at(beacon_id) = bm.str();
					}
				catch (...) {};
			}

			last_seen[beacon_id] = dojo.unix_timestamp();
		}

	}
}

Beacon::Beacon(asio::io_context& io_context,
		const asio::ip::address& multicast_address)
	: endpoint_(multicast_address, dojo.presence.multicast_port),
		socket_(io_context, endpoint_.protocol()),
		timer_(io_context)
{
	do_send();
}

void Beacon::do_send()
{
	std::ostringstream os;
	message_ = dojo.presence.ConstructMsg();

	socket_.async_send_to(
		asio::buffer(message_), endpoint_,
		[this](std::error_code ec, std::size_t /*length*/)
			{
				do_timeout();
			});
}

void Beacon::do_timeout()
{
	timer_.expires_after(std::chrono::seconds(5));
	timer_.async_wait(
		[this](std::error_code ec)
		{
			if (!ec)
				do_send();
		});
}

Listener::Listener(asio::io_context& io_context,
		const asio::ip::address& listen_address,
		const asio::ip::address& multicast_address)
	: socket_(io_context)
{
	// create socket so that multiple may be bound to the same address
	asio::ip::udp::endpoint listen_endpoint(
		listen_address, dojo.presence.multicast_port);
	socket_.open(listen_endpoint.protocol());
	socket_.set_option(asio::ip::udp::socket::reuse_address(true));
	socket_.bind(listen_endpoint);

	// join multicast group
	socket_.set_option(
		asio::ip::multicast::join_group(multicast_address));

	do_receive();
}

void Listener::do_receive()
{
	if (dojo.host_status != 0 || emu.render())
		return;

	socket_.async_receive_from(
		asio::buffer(data_), beacon_endpoint_,
		[this](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				dojo.presence.ListenerAction(beacon_endpoint_, data_.data(), length);

				do_receive();
			}
		});
}
