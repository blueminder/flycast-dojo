#include "dojo.h"
#include "deps/base64.h"

// connects to relay server
void RelayClient::ConnectRelayServer()
{
	struct hostent *mm_host;
	mm_host = gethostbyname(target_hostname.data());
	if (mm_host == nullptr)
	{
		ERROR_LOG(NETWORK, "Failed to resolve relay host: %s", target_hostname.c_str());
		return;
	}

	mms_addr.sin_family = AF_INET;
	mms_addr.sin_port = htons((u16)cfgLoadInt("dojo", "RelayPort", 8001));
	memcpy(&mms_addr.sin_addr, mm_host->h_addr_list[0], mm_host->h_length);
	std::string ip_address = inet_ntoa(*((in_addr *)mm_host->h_addr));
	config::NetworkServer.set(ip_address);

	INFO_LOG(NETWORK, "Connecting to Relay");
}

void RelayClient::SendHostMsg()
{
	std::string b64_game_name = to_base64(dojo.game_name.data());
	std::string mm_msg = "host|" + b64_game_name;
	outgoing_msgs.push_back(mm_msg);
	// std::cout << "To Relay: " << mm_msg << std::endl;
	NOTICE_LOG(NETWORK, "To Relay: %s", mm_msg.data());
}

void RelayClient::SendGuestMsg()
{
	std::string relay_key = cfgLoadStr("dojo", "RelayKey", "");

	while (relay_key.empty() && !disconnect_toggle)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	if (disconnect_toggle)
		return;

	std::string mm_msg = "rjoin|" + relay_key;
	outgoing_msgs.push_back(mm_msg);
	// std::cout << "To Relay: " << mm_msg << std::endl;
	NOTICE_LOG(NETWORK, "To Relay: %s", mm_msg.data());
}

sock_t RelayClient::CreateAndBind(int port)
{
	sock_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!VALID(sock))
	{
		ERROR_LOG(NETWORK, "Cannot create server socket");
		return sock;
	}
	int option = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((u16)port);

	if (::bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		ERROR_LOG(NETWORK, "Dojo Relay Client: bind() failed. errno=%d", get_last_error());
		CloseSocket(sock);
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool RelayClient::CreateLocalSocket(int port)
{
	if (!VALID(local_socket))
		local_socket = CreateAndBind(port);

	return VALID(local_socket);
}

bool RelayClient::Init()
{
	start_game = false;
	disconnect_toggle = false;
	key_shown = false;
	hole_punched = false;
	ping_test_start = 0;
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error());
		return false;
	}
#endif

	return CreateLocalSocket(config::GGPOPort);
}

void RelayClient::ClientLoop()
{
	isLoopStarted = true;

	while (!disconnect_toggle)
	{
		struct sockaddr_in sender;
		socklen_t senderlen = sizeof(sender);
		char buffer[256];
		memset(buffer, 0, 256);
		int bytes_read = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &senderlen);
		if (bytes_read)
		{
			if (memcmp("PING", buffer, 4) == 0)
			{
				opponent_addr.sin_family = AF_INET;
				opponent_addr.sin_port = sender.sin_port;

				char opponent_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(sender.sin_addr), opponent_ip, INET_ADDRSTRLEN);
				inet_pton(AF_INET, opponent_ip, &opponent_addr.sin_addr);

				opponent_server = std::string(opponent_ip, strlen(opponent_ip));
				opponent_port = htons(sender.sin_port);

				std::cout << "(PING) Opponent assigned to " << opponent_server << " " << opponent_port << std::endl;
				std::cout << "Received " << std::string(buffer, strlen(buffer)) << " from opponent" << std::endl;
				hole_punched = true;

				char buffer_copy[256];
				memcpy(buffer_copy, buffer, 256);
				buffer_copy[1] = 'O';

				std::string buffer_s = std::string(buffer_copy, strlen(buffer_copy));
				// sendto(local_socket, (const char *)buffer_copy, strlen(buffer_copy), 0, (struct sockaddr *)&sender, senderlen);
				memset(buffer, 0, 256);

				ping_msgs.push_back(buffer_s);
			}

			if (memcmp("PONG", buffer, 4) == 0)
			{
				opponent_addr.sin_family = AF_INET;
				opponent_addr.sin_port = sender.sin_port;

				char opponent_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(sender.sin_addr), opponent_ip, INET_ADDRSTRLEN);
				inet_pton(AF_INET, opponent_ip, &opponent_addr.sin_addr);

				opponent_server = std::string(opponent_ip, strlen(opponent_ip));
				opponent_port = htons(sender.sin_port);

				std::cout << "(PING) Opponent assigned to " << opponent_server << " " << opponent_port << std::endl;

				int rnd_num_cmp = atoi(buffer + 5);
				std::cout << "Received PONG " << rnd_num_cmp << " from opponent" << std::endl;
				hole_punched = true;
				uint64_t ret_timestamp = dojo.UnixTimestamp();

				if (ping_send_ts.count(rnd_num_cmp) == 1)
				{
					uint64_t rtt = ret_timestamp - ping_send_ts[rnd_num_cmp];
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

				std::cout << "AVG PING MS " << avg_ping_ms << std::endl;
			}

			// relay ping response
			else if (memcmp("RPONG", buffer, 5) == 0)
			{
				target_addr.sin_family = AF_INET;
				target_addr.sin_port = sender.sin_port;

				char target_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(sender.sin_addr), target_ip, INET_ADDRSTRLEN);
				inet_pton(AF_INET, target_ip, &target_addr.sin_addr);

				target_server = std::string(target_ip, strlen(target_ip));
				target_port = htons(sender.sin_port);

				std::string target_s = target_server + ":" + std::to_string(target_port);

				int rnd_num_cmp = atoi(buffer + 5);
				uint64_t ret_timestamp = dojo.UnixTimestamp();

				if (target_ping_send_ts[target_s].count(rnd_num_cmp) == 1)
				{
					uint64_t rtt = ret_timestamp - target_ping_send_ts[target_s][rnd_num_cmp];
					INFO_LOG(NETWORK, "Received PONG %d, RTT: %d ms", rnd_num_cmp, rtt);
					// std::cout << "Received RPONG " << rnd_num_cmp << " from " << target_server << " RTT: " << rtt << " ms" << std::endl;

					target_ping_rtt[target_s].push_back(rtt);

					if (target_ping_rtt[target_s].size() > 1)
					{
						target_avg_ping_ms[target_s] = std::accumulate(target_ping_rtt[target_s].begin(), target_ping_rtt[target_s].end(), 0.0) / target_ping_rtt[target_s].size();
					}
					else
					{
						target_avg_ping_ms[target_s] = rtt;
					}

					if (target_ping_rtt[target_s].size() > 5)
						target_ping_rtt[target_s].clear();

					target_ping_send_ts[target_s].erase(rnd_num_cmp);
				}
				// std::cout << "TARGET AVG PING MS " << target_s << " " << target_avg_ping_ms[target_s] << std::endl;
			}

			else if (memcmp("NOKEY", buffer, 5) == 0)
			{
				std::string received = std::string(buffer, 6);
				cfgSetVirtual("dojo", "RelayKey", received);
				disconnect_toggle = true;
			}
			else if (memcmp("MAXCN", buffer, 5) == 0)
			{
				std::string received = std::string(buffer, 6);
				cfgSetVirtual("dojo", "RelayKey", received);
				disconnect_toggle = true;
			}
			else if (memcmp("START", buffer, 5) == 0)
			{
				if (!cfgLoadBool("network", "ActAsServer", "no"))
				{
					std::cout << "Hole punching failed. Using relay." << std::endl;
					NOTICE_LOG(NETWORK, "Hole punching failed. Using relay.");
				}

				start_game = true;
				disconnect_toggle = true;
			}
			else if (bytes_read == 6)
			{
				std::string received = std::string(buffer, 6);
				cfgSetVirtual("dojo", "RelayKey", received);
				// std::cout << "Received Key: " << received << std::endl;
				NOTICE_LOG(NETWORK, "Received Key: %s", received.data());
			}
			else if (memcmp("OPPADDR", buffer, 7) == 0)
			{
				std::string data = std::string(buffer + 8, strlen(buffer + 5));
				std::vector<std::string> opp;
				dojo.Split(data, ':', opp);

				opponent_addr.sin_family = AF_INET;
				opponent_addr.sin_port = htons((u16)std::stol(opp[1]));
				inet_pton(AF_INET, opp[0].data(), &opponent_addr.sin_addr);

				// std::cout << "(OPPADDR) Opponent assigned to " << opp[0] << " " << opp[1] << std::endl;

				std::string quark = opp[2];
				config::Quark = opp[2];
				cfgSetVirtual("dojo", "Quark", quark);

				// std::cout << "(OPPADDR) Quark assigned to " << opp[2] << std::endl;

				if (cfgLoadBool("dojo", "RelayForceTunnel", false))
				{
					start_game = true;
					disconnect_toggle = true;
				}
				else
				{
					ping_test_start = dojo.UnixTimestamp();
					// std::cout << "PING TEST START " << ping_test_start << std::endl;
					NOTICE_LOG(NETWORK, "PING TEST START %u", ping_test_start);

					auto avg_ping = GetOpponentAvgPing(1);
				}
			}
		}

		std::vector<std::string> active_ping_targets;
		for (auto it = target_ping_msgs.begin(); it != target_ping_msgs.end(); ++it)
		{
			if ((it->second).size() > 0)
				active_ping_targets.push_back(it->first);
		}

		for (std::string ping_target : active_ping_targets)
		{
			std::vector<std::string> target_sock;
			dojo.Split(ping_target, ':', target_sock);

			std::string target_address = target_sock[0];
			std::string target_port = std::to_string(config::RelayPort.get());
			if (target_sock.size() > 1)
				target_port = target_sock[1];

			struct hostent *target_host;
			target_host = gethostbyname(target_address.data());
			if (target_host == nullptr)
			{
				ERROR_LOG(NETWORK, "Failed to resolve target: %s", target_address.c_str());
				target_ping_msgs[ping_target].clear();
				continue;
			}

			target_addr.sin_family = AF_INET;
			target_addr.sin_port = htons((u16)std::stol(target_port));
			memcpy(&target_addr.sin_addr, target_host->h_addr_list[0], target_host->h_length);

			while (target_ping_msgs[ping_target].size() > 0)
			{
				std::string ping_msg = target_ping_msgs[ping_target].front();
				sendto(local_socket, (const char *)ping_msg.data(), strlen(ping_msg.data()), 0, (const struct sockaddr *)&target_addr, sizeof(target_addr));
				// std::cout << "Sent " << ping_msg << " to target " << ping_target << std::endl;
				target_ping_msgs[ping_target].pop_front();
			}
		}

		if (opponent_addr.sin_port > 0)
		{
			while (ping_msgs.size() > 0)
			{
				std::string ping_msg = ping_msgs.front();
				sendto(local_socket, (const char *)ping_msg.data(), strlen(ping_msg.data()), 0, (const struct sockaddr *)&opponent_addr, sizeof(opponent_addr));
				// std::cout << "Sent " << ping_msg << " to opponent" << std::endl;
				ping_msgs.pop_front();
			}
		}

		if (hole_punched &&
			ping_msgs.empty())
		{
			config::NetworkServer = opponent_server;
			config::GGPORemotePort = opponent_port;
			std::cout << "Hole punching succeeded." << std::endl;
			NOTICE_LOG(NETWORK, "Hole punching succeeded.");

			start_game = true;
			disconnect_toggle = true;
		}

		if (ping_test_start > 0 &&
			(dojo.UnixTimestamp() > (ping_test_start + 2000)) &&
			!hole_punched &&
			ping_msgs.empty())
		{
			std::cout << "Hole punching failed. Using relay." << std::endl;
			NOTICE_LOG(NETWORK, "Hole punching failed. Using relay.");

			// std::cout << "PING TEST END " << dojo.UnixTimestamp() << std::endl;
			NOTICE_LOG(NETWORK, "PING TEST END %u", dojo.UnixTimestamp());
			ping_test_start = 0;

			start_game = true;
			disconnect_toggle = true;
		}

		while (outgoing_msgs.size() > 0)
		{
			std::string out_msg = outgoing_msgs.front();
			sendto(local_socket, (const char *)out_msg.data(), strlen(out_msg.data()), 0, (const struct sockaddr *)&mms_addr, sizeof(mms_addr));
			outgoing_msgs.pop_front();
		}
	}
}

void RelayClient::ClientThread()
{
	if (!connect_started)
	{
		Init();
		ConnectRelayServer();
	}
	ClientLoop();
	CloseSocket(local_socket);
}

void RelayClient::PingThread()
{
	Init();
	ClientLoop();
	CloseSocket(local_socket);
}

void RelayClient::AddToRelayAddressHistory(std::string address)
{
	std::vector<std::string> relay_addresses = GetRelayAddressHistory();
	if (std::find(relay_addresses.begin(), relay_addresses.end(), address) == relay_addresses.end())
	{
		cfgSaveStr("dojo", "RelayAddressHistory", config::RelayAddressHistory.get() + address + ";");
	}
}

std::vector<std::string> RelayClient::GetRelayAddressHistory()
{
	std::string history = config::RelayAddressHistory.get();
	std::vector<std::string> relay_addresses;
	dojo.Split(history, ';', relay_addresses);
	return relay_addresses;
}

// http://www.concentric.net/~Ttwang/tech/inthash.htm
unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
	a = a - b;
	a = a - c;
	a = a ^ (c >> 13);
	b = b - c;
	b = b - a;
	b = b ^ (a << 8);
	c = c - a;
	c = c - b;
	c = c ^ (b >> 13);
	a = a - b;
	a = a - c;
	a = a ^ (c >> 12);
	b = b - c;
	b = b - a;
	b = b ^ (a << 16);
	c = c - a;
	c = c - b;
	c = c ^ (b >> 5);
	a = a - b;
	a = a - c;
	a = a ^ (c >> 3);
	b = b - c;
	b = b - a;
	b = b ^ (a << 10);
	c = c - a;
	c = c - b;
	c = c ^ (b >> 15);
	return c;
}

std::string RelayClient::RandomHexString(int length, int seed)
{
	srand(seed);

	char hex_out[1024] = {0};
	char hex_chars[] =
		{'0', '1', '2', '3', '4', '5', '6', '7',
		 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	for (int i = 0; i < length; i++)
	{
		hex_out[i] = hex_chars[rand() % 16];
	}

	std::string x_out(hex_out, strlen(hex_out));

	return x_out;
}

// udp ping, seeds with random number
int RelayClient::PingOpponent(int add_to_seed)
{
	unsigned long seed = mix(clock(), time(NULL), getpid());
	srand(seed + add_to_seed);
	int rnd_num_cmp = rand() * 1000 + 1;
	// std::cout << "PING " << rnd_num_cmp << std::endl;

	if (ping_send_ts.count(rnd_num_cmp) == 0)
	{
		std::stringstream ping_ss("");
		ping_ss << "PING " << rnd_num_cmp << " " << RandomHexString(32, seed);
		std::string to_send_ping = ping_ss.str();

		ping_msgs.push_back(to_send_ping);
		INFO_LOG(NETWORK, "Sent %s to Opponent", to_send_ping.data());

		uint64_t current_timestamp = dojo.UnixTimestamp();
		ping_send_ts.emplace(rnd_num_cmp, current_timestamp);
	}

	// last ping key
	return rnd_num_cmp;
}

uint64_t RelayClient::GetOpponentAvgPing(int num_requests)
{
	for (int i = 0; i < num_requests; i++)
	{
		PingOpponent(i);
	}

	return avg_ping_ms;
}

// udp ping, seeds with random number
int RelayClient::PingTarget(std::string target, int add_to_seed)
{
	std::vector<std::string> target_sock;
	dojo.Split(target, ':', target_sock);

	std::string target_address = target_sock[0];
	std::string target_port = std::to_string(config::RelayPort.get());
	if (target_sock.size() > 1)
		target_port = target_sock[1];

	struct hostent *target_host;
	target_host = gethostbyname(target_address.data());
	if (target_host == nullptr)
	{
		ERROR_LOG(NETWORK, "Failed to resolve target: %s", target_address.c_str());
		return 0;
	}
	std::string target_ip = inet_ntoa(*((in_addr *)target_host->h_addr));

	std::string target_s = target_ip + ":" + target_port;

	unsigned long seed = mix(clock(), time(NULL), getpid());
	srand(seed + add_to_seed);
	int rnd_num_cmp = rand() * 1000 + 1;

	// std::cout << "PING " << rnd_num_cmp << std::endl;

	if (target_ping_send_ts[target_s].count(rnd_num_cmp) == 0)
	{
		std::stringstream ping_ss("");
		ping_ss << "RPING " << rnd_num_cmp << " " << RandomHexString(32, seed);
		std::string to_send_ping = ping_ss.str();

		target_ping_msgs[target_s].push_back(to_send_ping);
		INFO_LOG(NETWORK, "Sent %s to Target", to_send_ping.data());

		uint64_t current_timestamp = dojo.UnixTimestamp();
		target_ping_send_ts[target_s].emplace(rnd_num_cmp, current_timestamp);
	}

	// last ping key
	return rnd_num_cmp;
}

uint64_t RelayClient::RepeatTargetPing(std::string target, int num_requests)
{
	for (int i = 0; i < num_requests; i++)
	{
		PingTarget(target, i);
	}

	return target_avg_ping_ms[target];
}

std::string RelayClient::GetTargetStr(std::string target)
{
	std::vector<std::string> target_sock;
	dojo.Split(target, ':', target_sock);

	std::string target_address = target_sock[0];
	std::string target_port = std::to_string(config::RelayPort.get());
	if (target_sock.size() > 1)
		target_port = target_sock[1];

	struct hostent *target_host;
	target_host = gethostbyname(target_address.data());
	if (target_host == nullptr)
	{
		ERROR_LOG(NETWORK, "Failed to resolve target: %s", target_address.c_str());
		return "";
	}
	std::string target_ip = inet_ntoa(*((in_addr *)target_host->h_addr));

	std::string target_s = target_ip + ":" + target_port;

	return target_s;
}

uint64_t RelayClient::GetTargetAvgPing(std::string target)
{
	std::string target_s = GetTargetStr(target);
	// unresolvable target: sort last
	if (target_s.empty())
		return 999999;

	return target_avg_ping_ms[target_s];
}

std::string RelayClient::AssignClosestRelay()
{
	bool existing_client = true;

	if (!isLoopStarted)
	{
		existing_client = false;
		try
		{
			dojo.relay_client.disconnect_toggle = false;
			std::thread t2(&RelayClient::PingThread, std::ref(dojo.relay_client));
			t2.detach();
		}
		catch (std::exception &)
		{
		}
	}

	while (!isLoopStarted)
		;

	auto test_start = dojo.UnixTimestamp();

	std::vector<std::string> relay_servers = ReadRelayJson();

	int ping_iterations = 5;

	for (auto server : relay_servers)
	{
		dojo.relay_client.RepeatTargetPing(server, ping_iterations);
	}

	while ((test_start + 1000) > dojo.UnixTimestamp())
		;

	std::vector<std::pair<std::string, int>> relay_rtts;

	for (auto server : relay_servers)
	{
		auto avg_ping = dojo.relay_client.GetTargetAvgPing(server);
		relay_rtts.push_back(std::make_pair(server, avg_ping));
	}

	std::sort(relay_rtts.begin(), relay_rtts.end(), [=](std::pair<std::string, int> &a, std::pair<std::string, int> &b)
			  { return a.second < b.second; });

	for (auto rtt_entry : relay_rtts)
	{
		std::cout << rtt_entry.first << " " << rtt_entry.second << std::endl;
	}

	if (!existing_client)
	{
		disconnect_toggle = true;
		isLoopStarted = false;
	}

	std::string closest = relay_rtts.at(0).first;
	std::cout << "Closest Relay: " << closest << std::endl;

	for (auto server : relay_servers)
	{
		target_ping_msgs.clear();
		target_ping_send_ts.clear();
		target_ping_rtt.clear();
		target_avg_ping_ms.clear();
	}

	cfgSaveStr("dojo", "RelayServer", closest);
	config::RelayServer = closest;
	return closest;
}

std::vector<std::string> RelayClient::ReadRelayJson()
{
	std::vector<std::string> servers;

	if (!ghc::filesystem::exists(get_writable_data_path("relays.json")))
	{
		dojo_file.DownloadFile(config::RelayJsonUrl.get(), "data", "", "");
		while (!ghc::filesystem::exists(get_writable_data_path("relays.json")))
			;
	}

	std::ifstream f(get_writable_data_path("relays.json"));
	auto data = nlohmann::json::parse(f);

	for (auto relay_entry : data["relays"])
	{
		servers.push_back(relay_entry["url"]);
	}

	return servers;
}
