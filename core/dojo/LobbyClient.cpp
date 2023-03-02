#include "DojoSession.hpp"

LobbyClient::LobbyClient()
{
	isLoopStarted = false;
	memset((void*)to_send, 0, 256);
};

// http://www.concentric.net/~Ttwang/tech/inthash.htm
unsigned long LobbyClient::mix(unsigned long a, unsigned long b, unsigned long c)
{
	a=a-b;  a=a-c;  a=a^(c >> 13);
	b=b-c;  b=b-a;  b=b^(a << 8);
	c=c-a;  c=c-b;  c=c^(b >> 13);
	a=a-b;  a=a-c;  a=a^(c >> 12);
	b=b-c;  b=b-a;  b=b^(a << 16);
	c=c-a;  c=c-b;  c=c^(b >> 5);
	a=a-b;  a=a-c;  a=a^(c >> 3);
	b=b-c;  b=b-a;  b=b^(a << 10);
	c=c-a;  c=c-b;  c=c^(b >> 15);
	return c;
}

std::string LobbyClient::random_hex_string(int length, int seed)
{
	srand(seed);

	char hex_out[1024] = { 0 };
	char hex_chars[] =
		{'0','1','2','3','4','5','6','7',
		 '8','9','A','B','C','D','E','F'};

	for (int i = 0; i < length; i++)
	{
		hex_out[i] = hex_chars[rand() % 16];
	}

	std::string x_out(hex_out, strlen(hex_out));

	return x_out;
}

// udp ping, seeds with random number
int LobbyClient::PingAddress(sockaddr_in target_addr, int add_to_seed)
{
	unsigned long seed = mix(clock(), time(NULL), getpid());
	srand(seed + add_to_seed);
	//srand(time(NULL));
	int rnd_num_cmp = rand() * 1000 + 1;

	if (ping_send_ts.count(rnd_num_cmp) == 0)
	{
		std::stringstream ping_ss("");
		ping_ss << "PING " << rnd_num_cmp << " " << random_hex_string(32, seed);
		std::string to_send_ping = ping_ss.str();

		sendto(local_socket, (const char*)to_send_ping.data(), strlen(to_send_ping.data()), 0, (const struct sockaddr*)&target_addr, sizeof(target_addr));
		INFO_LOG(NETWORK, "Sent %s", to_send_ping.data());

		uint64_t current_timestamp = dojo.unix_timestamp();
		ping_send_ts.emplace(rnd_num_cmp, current_timestamp);
	}

	// last ping key
	return rnd_num_cmp;
}

int LobbyClient::PingAddress(const char * ip_addr, int port, int add_to_seed)
{
	sockaddr_in target_addr;
	target_addr.sin_family = AF_INET;
	target_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, ip_addr, &target_addr.sin_addr);

	return PingAddress(target_addr, add_to_seed);
}

uint64_t LobbyClient::GetAvgPing(const char * ip_addr, int port)
{
	for (int i = 0; i < 5; i++)
	{
		PingAddress(ip_addr, port, i);
	}

	return avg_ping_ms;
}

void LobbyClient::SendMsg(std::string msg, std::string ip, int port)
{
	unsigned long add_to_seed = (unsigned long)msg.size() + msgs_sent;
	unsigned long seed = mix(clock(), time(NULL), getpid());
	srand(seed + add_to_seed);

	std::string uid_rand = random_hex_string(16, seed);
	if (msg.rfind("JOIN ", 0) == 0)
		join_rand = uid_rand;
	msg += ":" + uid_rand;

	sockaddr_in target_addr;
	target_addr.sin_family = AF_INET;
	target_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, ip.data(), &target_addr.sin_addr);

	for (int i = 0; i < config::PacketsPerFrame; i++)
		sendto(local_socket, (const char*)msg.data(), strlen(msg.data()), 0, (const struct sockaddr*)&target_addr, sizeof(target_addr));

	msgs_sent++;

	INFO_LOG(NETWORK, "Message Sent: %s", msg.data());
}

void LobbyClient::EndSession()
{
	dojo.presence.player_count = 0;
	isLoopStarted = false;

	CloseSocket(local_socket);
#ifdef _WIN32
	WSACleanup();
#endif

	INFO_LOG(NETWORK, "Lobby Disconnected.");
}

sock_t LobbyClient::CreateAndBind(int port)
{
	sock_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!VALID(sock))
	{
		ERROR_LOG(NETWORK, "Cannot create server socket");
		return sock;
	}
	int option = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, sizeof(option));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons((u16)port);

	if (::bind(sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
	{
		ERROR_LOG(NETWORK, "Lobby UDP Server: bind() failed. errno=%d", get_last_error_n());
		CloseSocket(sock);
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool LobbyClient::CreateLocalSocket(int port)
{
	if (!VALID(local_socket))
		local_socket = CreateAndBind(port);

	return VALID(local_socket);
}

bool LobbyClient::Init(bool hosting)
{
	if (!config::EnableLobby)
		return false;
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error_n());
		return false;
	}
#endif

if (hosting)
{

#ifndef __ANDROID__
	auto iadd = dojo.GetLocalInterfaces();
	for (auto ia : iadd)
	{
		auto ip_str = ia.to_string();
		auto dot_found = ip_str.find(".");
		if (dot_found == std::string::npos)
			continue;

		DEBUG_LOG(NETWORK, "IP: %s\n", ip_str.data());
		local_ips.push_back(ip_str);
	}
#endif

	return CreateLocalSocket(stoi(config::DojoServerPort));
}
else
	return CreateLocalSocket(0);
}

std::string LobbyClient::longestCommonPrefix(std::vector<std::string>& str) {
	int n = str.size();
	if(n==0) return "";

	std::string ans  = "";
	sort(begin(str), end(str));
	std::string a = str[0];
	std::string b = str[n-1];

	for(int i=0; i<a.size(); i++){
		if(a[i]==b[i]){
			ans = ans + a[i];
		}
		else{
			break;
		}
	}

	return ans;
}

void LobbyClient::ClientLoop()
{
	isLoopStarted = true;

	while (!dojo.presence.lobby_disconnect_toggle)
	{
		struct sockaddr_in sender;
		socklen_t senderlen = sizeof(sender);
		char buffer[256];
		memset(buffer, 0, 256);
		int bytes_read = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderlen);
		if (bytes_read)
		{

			if (memcmp("PING", buffer, 4) == 0)
			{
				char buffer_copy[256];
				memcpy(buffer_copy, buffer, 256);
				buffer_copy[1] = 'O';
				sendto(local_socket, (const char*)buffer_copy, strlen(buffer_copy), 0, (struct sockaddr*)&sender, senderlen);
				memset(buffer, 0, 256);
			}

			if (memcmp("PONG", buffer, 4) == 0)
			{
				int rnd_num_cmp = atoi(buffer + 5);
				uint64_t ret_timestamp = dojo.unix_timestamp();

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

			}

			// guests
			if (dojo.host_status == 4 && memcmp("JOINED", buffer, 6) == 0)
			{
				if (dojo.presence.hosting_lobby)
					continue;

				std::string data = std::string(buffer + 7, strlen(buffer + 7));
				std::vector<std::string> args = stringfix::split(":", data);

				int player_num = std::stoi(args[0]);
				auto player_name = args[1];
				auto player_ip = args[2];
				auto player_ggpo_port = std::stoi(args[3]);
				auto uid_rand = args[4];

				if (joined_msg_uids.count(uid_rand) == 1 && used_port_nums.count(player_num) == 1)
					continue;

				joined_msg_uids.insert(uid_rand);

				std::cout << "RECEIVED: " << std::string(buffer, strlen(buffer)) << std::endl;

				Player joined = { player_name, player_ip, 0, player_num, player_ggpo_port };

				auto it = std::find_if(dojo.presence.players.begin(), dojo.presence.players.end(),
					[player_ip, player_ggpo_port](Player& p)
					{
						return p.ip == player_ip && p.ggpo_port == player_ggpo_port;
					});

				if (it == dojo.presence.players.end())
				{
					dojo.presence.players.push_back(joined);
					used_port_nums.insert(joined.port_num);
					std::cout << "PLAYER " << std::to_string(joined.port_num) << " ADDED: " << joined.ip << ":" << std::to_string(joined.ggpo_port) << " " << std::to_string(joined.listen_port) << "  " << std::to_string(dojo.presence.players.size()) << std::endl;

					if (uid_rand == join_rand)
					{
						//config::PlayerNum = player_num;
						//std::cout << "ASSIGNED PLAYER NUMBER " << std::to_string(config::PlayerNum.get()) << std::endl;

						// if request comes from same machine with identical ggpo port
						// automatically assign different ggpo port
						if (player_ggpo_port != config::GGPOPort.get())
						{
							config::GGPOPort = player_ggpo_port;
							std::cout << "ASSIGNED GGPO PORT " << std::to_string(config::GGPOPort.get()) << std::endl;
						}
					}

					dojo.presence.player_count++;

					if (dojo.presence.player_count == MAX_PLAYERS)
					{
						for (auto it = dojo.presence.players.begin(); it != dojo.presence.players.end(); ++it)
						{
							dojo.presence.player_ips[it->port_num] = it->ip;
							dojo.presence.player_ggpo_ports[it->port_num] = it->ggpo_port;
							dojo.presence.player_names[it->port_num] = it->name;
						}

						StartSessionOnReady(false);
					}
				}
			}

			// host
			if (dojo.host_status == 1 && memcmp("JOIN", buffer, 4) == 0)
			{

				if (dojo.presence.player_count >= MAX_PLAYERS)
					continue;

				auto ip = std::string(inet_ntoa(sender.sin_addr));
				auto listen_port = htons(sender.sin_port);
				auto current_target = ip + ":" + std::to_string(listen_port);

				if (dojo.presence.targets.count(current_target) == 0)
				{
					dojo.presence.targets.insert(current_target);

					std::string data = std::string(buffer + 5, strlen(buffer + 5));
					std::vector<std::string> args = stringfix::split(":", data);

					auto name = args[0];
					auto ggpo_port = std::stoi(args[1]);

					auto uid_rand = args[2];
					if (join_msg_uids.count(uid_rand) == 1)
						continue;

					join_msg_uids.insert(uid_rand);

					std::cout << "RECEIVED: " << std::string(buffer, strlen(buffer)) << std::endl;

					auto it = std::find_if(dojo.presence.players.begin(), dojo.presence.players.end(),
					[ip, ggpo_port](Player& p)
					{
						return p.ip == ip && p.ggpo_port == ggpo_port;
					});

					bool same_ip_as_host = false;
					for (auto it = local_ips.begin(); it != local_ips.end(); ++it)
					{
						if (*it == ip)
							same_ip_as_host = true;
					}

					// if request comes from same machine with identical ggpo port
					// automatically assign different ggpo port
					if (it != dojo.presence.players.end() ||
						same_ip_as_host && ggpo_port == config::GGPOPort.get())
					{
						ggpo_port = ggpo_port - dojo.presence.player_count;
					}

					Player player;
					player = { name, ip, listen_port, dojo.presence.player_count, ggpo_port };

					// send previously joined players to newly joined one
					if (player.port_num > 1)
					{
						for (auto it = dojo.presence.players.begin(); it != dojo.presence.players.end(); ++it)
						{
							std::string prev_response = std::string("JOINED " + std::to_string(it->port_num) + ":" + it->name + ":" + it->ip + ":" + std::to_string(it->ggpo_port));
							//for (int i = 0; i<config::PacketsPerFrame; i++)
								SendMsg(prev_response, ip, listen_port);
						}
					}

					dojo.presence.players.push_back(player);
					std::cout << "PLAYER " << std::to_string(player.port_num) << " ADDED: " << player.ip << ":" << std::to_string(player.ggpo_port) << " " << std::to_string(player.listen_port) << "  " << std::to_string(dojo.presence.players.size()) << std::endl;

					std::string response = std::string("JOINED " + std::to_string(player.port_num) + ":" + player.name + ":" + player.ip + ":" + std::to_string(player.ggpo_port) + ":" + uid_rand);

					//sendto(local_socket, (const char*)response.data(), strlen(response.data()), 0, (struct sockaddr*)&sender, senderlen);
					for (auto it = dojo.presence.targets.begin(); it != dojo.presence.targets.end(); ++it)
					{
						std::vector<std::string> ip_port = stringfix::split(":", *it);
						//for (int i = 0; i<config::PacketsPerFrame; i++)
							SendMsg(response, ip_port[0], std::stoi(ip_port[1]));
					}

					dojo.presence.player_count++;

					if (dojo.presence.player_count == MAX_PLAYERS)
					{
						// find common IP prefix with host, share with guests
						std::string lcp = longestCommonPrefix(dojo.presence.player_ips);
						lcp = lcp.substr(0, lcp.find_last_of("."));

						std::string assigned_ip = "";

						auto substring = lcp;
						const auto iter = std::find_if(local_ips.begin(), local_ips.end(),
							[&substring](std::string str) {
								return str.find(substring) == 0;
							}
						);

						if (iter != local_ips.end())
						{
							assigned_ip = *iter;
						}

						if (assigned_ip == "")
						{
							//assigned_ip = dojo.GetExternalIP();
							assigned_ip = "127.0.0.1";
						}

						Player host_player = { config::PlayerName, assigned_ip, std::stoi(config::DojoServerPort.get()), 0, config::GGPOPort.get() };
						dojo.presence.players.push_back(host_player);
						std::cout << "PLAYER " << std::to_string(host_player.port_num) << " ADDED: " << host_player.ip << ":" << std::to_string(host_player.ggpo_port) << " " << std::to_string(host_player.listen_port) << "  " << std::to_string(dojo.presence.players.size()) << std::endl;

						msgs_sent++;
						for (auto it = dojo.presence.players.begin(); it != dojo.presence.players.end(); ++it)
						{
							std::string host_response = std::string("JOINED " + std::to_string(host_player.port_num) + ":" + host_player.name + ":" + host_player.ip + ":" + std::to_string(host_player.ggpo_port));
							SendMsg(host_response, it->ip, it->listen_port);
						}

						for (auto it = dojo.presence.players.begin(); it != dojo.presence.players.end(); ++it)
						{
							dojo.presence.player_ips[it->port_num] = it->ip;
							dojo.presence.player_ggpo_ports[it->port_num] = it->ggpo_port;
							dojo.presence.player_names[it->port_num] = it->name;
						}
					}

					StartSessionOnReady(true);
				}
			}
		}
	}
}

void LobbyClient::StartSessionOnReady(bool hosting)
{
	if(dojo.presence.player_count == MAX_PLAYERS)
	{
		config::OpponentName = dojo.presence.player_names[1];

		try
		{
			dojo.commandLineStart = true;

			{
				if (hosting)
				{
					config::GGPOPort = dojo.presence.player_ggpo_ports[0];

					config::NetworkServer = dojo.presence.player_ips[1];
					config::GGPORemotePort = dojo.presence.player_ggpo_ports[1];
				}
				else
				{
					config::NetworkServer = dojo.presence.player_ips[0];
					config::GGPORemotePort = dojo.presence.player_ggpo_ports[0];
				}
			}

			dojo.PlayMatch = false;
			config::Receiving = false;

			config::DojoEnable = true;
			config::GGPOEnable = true;

			if (hosting)
			{
				config::DojoActAsServer = true;
				config::ActAsServer = true;
				dojo.host_status = 2;
			}
			else
			{
				config::DojoActAsServer = false;
				config::ActAsServer = false;
				dojo.host_status = 5;
			}

			SaveSettings();

			std::cout << ":" << std::to_string(config::GGPOPort) << std::endl;
			std::cout << config::NetworkServer.get() << ":" << std::to_string(config::GGPORemotePort) << std::endl << std::endl;

			dojo.presence.player_names.clear();
			dojo.presence.player_ips.clear();
			dojo.presence.player_ggpo_ports.clear();
			dojo.presence.players.clear();
			dojo.presence.player_count = 1;

			dojo.lobby_active = false;
			dojo.presence.lobby_disconnect_toggle = true;
			dojo.lobby_launch = true;

			gui_state = GuiState::Closed;
			gui_start_game(settings.content.path);
		}
		catch (...) { }


	}
}

void LobbyClient::CloseLocalSocket()
{
	CloseSocket(local_socket);
}

void LobbyClient::ClientThread()
{
	Init(dojo.presence.hosting_lobby);
	ClientLoop();
	EndSession();
}
