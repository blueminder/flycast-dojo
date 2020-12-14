#pragma once

#include <map>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "stdclass.h"

class DojoLobby
{
public:
	DojoLobby() {};

	void BeaconThread();
	void ListenerThread();
	void ListenerAction(asio::ip::udp::endpoint beacon_endpoint, char* msgbuf, int length);

	std::string ConstructMsg();

	std::map<std::string, std::string> active_beacons;
	std::map<std::string, int> active_beacon_ping;
	std::map<std::string, long> last_seen;
};

constexpr short multicast_port = 7776;

class Beacon
{
public:
	Beacon(asio::io_context& io_context,
		const asio::ip::address& multicast_address);

private:
	void do_send();
	void do_timeout();

private:
	asio::ip::udp::endpoint endpoint_;
	asio::ip::udp::socket socket_;
	asio::steady_timer timer_;
	int message_count_;
	std::string message_;
};

class Listener
{
public:
	Listener(asio::io_context& io_context,
		const asio::ip::address& listen_address,
		const asio::ip::address& multicast_address);

private:
	void do_receive();

	asio::ip::udp::socket socket_;
	asio::ip::udp::endpoint beacon_endpoint_;
	std::array<char, 1024> data_;
};

