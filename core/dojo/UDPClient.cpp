#include "DojoSession.hpp"

UDPClient::UDPClient()
{
	isLoopStarted = false;
	write_out = false;
	memset((void*)to_send, 0, 256);
	last_sent = "";
	opponent_disconnected = false;
	name_acknowledged = false;
};

// connects to matchmaking server
void UDPClient::ConnectMMServer()
{

	struct hostent* mm_host;
	mm_host = gethostbyname(config::MatchmakingServerAddress.get().data());

	sockaddr_in mms_addr;
	mms_addr.sin_family = AF_INET;
	mms_addr.sin_port = htons((u16)std::stoul(config::MatchmakingServerPort));
	memcpy(&mms_addr.sin_addr, mm_host->h_addr_list[0], mm_host->h_length);
	//inet_pton(AF_INET, config::MatchmakingServerAddress.data(), &mms_addr.sin_addr);

	std::string mm_msg;
	if (dojo.hosting)
		mm_msg = "host:cmd:";
	else
	{
		while (dojo.MatchCode.empty())
			std::this_thread::sleep_for (std::chrono::milliseconds(1));
		mm_msg = "guest:cmd:" + dojo.MatchCode;
	}

	for (int i = 0; i < dojo.packets_per_frame; i++)
	{
		sendto(local_socket, (const char*)mm_msg.data(), strlen(mm_msg.data()), 0, (const struct sockaddr*)&mms_addr, sizeof(mms_addr));
	}
	INFO_LOG(NETWORK, "Connecting to Matchmaking Relay");
}

void UDPClient::SetHost(std::string host, int port)
{
	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, host.data(), &host_addr.sin_addr);
}

// for sending frame data during emulator game loop
int UDPClient::SendData(std::string data)
{
	// sets current send buffer
	write_out = true;
	memcpy((void*)to_send, data.data(), dojo.PayloadSize());
	write_out = false;

	if (dojo.client_input_authority)
		dojo.AddNetFrame((const char*)to_send);

	return 0;
}

// http://www.concentric.net/~Ttwang/tech/inthash.htm
unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
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

std::string UDPClient::random_hex_string(int length, int seed)
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
int UDPClient::PingAddress(sockaddr_in target_addr, int add_to_seed)
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

int UDPClient::PingAddress(const char * ip_addr, int port, int add_to_seed)
{
	sockaddr_in target_addr;
	target_addr.sin_family = AF_INET;
	target_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, ip_addr, &target_addr.sin_addr);

	return PingAddress(target_addr, add_to_seed);
}

uint64_t UDPClient::GetAvgPing(const char * ip_addr, int port)
{
	for (int i = 0; i < 5; i++)
	{
		PingAddress(ip_addr, port, i);
	}

	return avg_ping_ms;
}

uint64_t UDPClient::GetOpponentAvgPing(int num_requests)
{
	for (int i = 0; i < num_requests; i++)
	{
		PingAddress(opponent_addr, i);
	}

	return avg_ping_ms;
}

// for messages sent outside the game loop
void UDPClient::SendMsg(std::string msg, sockaddr_in target)
{
	for (int i = 0; i < dojo.packets_per_frame; i++)
	{
		sendto(local_socket, (const char*)msg.data(), strlen(msg.data()), 0, (const struct sockaddr*)&target, sizeof(target));
	}

	INFO_LOG(NETWORK, "Message Sent: %s", msg.data());
}

void UDPClient::SendSpectate()
{
	SendMsg("SPECTATE " + config::SpectatorPort.get(), host_addr);
}

void UDPClient::SendSpectateOK(sockaddr_in target_addr)
{
	SendMsg("OK SPECTATE", target_addr);
}

void UDPClient::StartSession()
{
	std::stringstream start_ss("");
	start_ss << "START " << dojo.delay
		<< " " << dojo.packets_per_frame
		<< " " << dojo.num_back_frames
		<< " " << config::PlayerName.get()
		<< " " << (ghc::filesystem::exists(dojo.net_save_path) && !config::IgnoreNetSave) ? 1 : 0;

	std::string to_send_start = start_ss.str();

	SendMsg(to_send_start, opponent_addr);

	INFO_LOG(NETWORK, "Session Started");
}

void UDPClient::SendDisconnect()
{
	SendMsg("DISCONNECT", opponent_addr);
}

void UDPClient::SendDisconnectOK()
{
	SendMsg("OK DISCONNECT", opponent_addr);
}

void UDPClient::SendPlayerName()
{
	int use_net_save = ghc::filesystem::exists(dojo.net_save_path) && !config::IgnoreNetSave;
	SendMsg("NAME " + config::PlayerName.get() + " " + std::to_string(use_net_save), opponent_addr);
}

void UDPClient::SendNameOK()
{
	SendMsg("OK NAME", opponent_addr);
}

void UDPClient::EndSession()
{
	gui_open_disconnected();
	emu.term();
	CloseSocket(local_socket);
#ifdef _WIN32
	WSACleanup();
#endif

	INFO_LOG(NETWORK, "Disconnected.");
}

sock_t UDPClient::CreateAndBind(int port)
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
		ERROR_LOG(NETWORK, "DojoSession UDP Server: bind() failed. errno=%d", get_last_error_n());
		CloseSocket(sock);
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool UDPClient::CreateLocalSocket(int port)
{
	if (!VALID(local_socket))
		local_socket = CreateAndBind(port);

	return VALID(local_socket);
}

bool UDPClient::Init(bool hosting)
{
	if (!config::DojoEnable)
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
		return CreateLocalSocket(stoi(config::DojoServerPort));
	}
	else
	{
		if (config::EnableMatchCode)
		{
			config::DojoServerIP = "";
		}
		else
		{
			while (config::DojoServerIP.get().empty());
			SetHost(config::DojoServerIP.get(), atoi(config::DojoServerPort.get().data()));
		}
		opponent_addr = host_addr;
		return CreateLocalSocket(0);
	}
}

void UDPClient::ClientLoop()
{
	isLoopStarted = true;

	while (!dojo.disconnect_toggle)
	{
		// if opponent detected, shoot packets at them
		if (opponent_addr.sin_port > 0)
		{
			if (memcmp(to_send, last_sent.data(), dojo.PayloadSize()) != 0)
			{
				// send payload until morale improves
				for (int i = 0; i < dojo.packets_per_frame; i++)
				{
					sendto(local_socket, (const char*)to_send, dojo.PayloadSize(), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				}

				if (config::Debug == DEBUG_SEND ||
					config::Debug == DEBUG_SEND_RECV ||
					config::Debug == DEBUG_ALL)
				{
					dojo.PrintFrameData("Sent", (u8*)to_send);
				}

				last_sent = std::string(to_send, to_send + dojo.PayloadSize());

				if (dojo.client_input_authority)
					dojo.AddNetFrame((const char*)to_send);
			}

			if (request_repeat &&
				!last_sent.empty())
			{
				sendto(local_socket, (const char*)last_sent.data(), dojo.PayloadSize(), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				//sendto(local_socket, "REP", strlen("REP"), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				request_repeat = false;
			}
		}

		struct sockaddr_in sender;
		socklen_t senderlen = sizeof(sender);
		char buffer[256];
		memset(buffer, 0, 256);
		int bytes_read = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderlen);
		if (bytes_read)
		{
			if (memcmp("CODE", buffer, 4) == 0)
			{
				config::MatchCode = std::string(buffer + 5, strlen(buffer + 5));
			}

			if (memcmp("OPPADDR", buffer, 7) == 0)
			{
				if (config::EnableMatchCode)
				{
					std::string data = std::string(buffer + 8, strlen(buffer + 5));
					std::vector<std::string> opp = stringfix::split(":", data);

					if (config::GGPOEnable)
					{
						config::NetworkServer = opp[0];

						opponent_addr.sin_family = AF_INET;
						opponent_addr.sin_port = htons((u16)std::stol(opp[1]));
						inet_pton(AF_INET, opp[0].data(), &opponent_addr.sin_addr);

						if (!config::ActAsServer.get())
						{
							host_addr.sin_port = htons((u16)std::stol(opp[1]));
							inet_pton(AF_INET, opp[0].data(), &host_addr.sin_addr);
						}

						SendPlayerName();
					}
					else
					{
						if (!config::DojoActAsServer)
						{
							config::DojoServerIP = opp[0];
							//config::DojoServerPort = opp[1];
						}

						opponent_addr.sin_family = AF_INET;
						opponent_addr.sin_port = htons((u16)std::stol(opp[1]));
						inet_pton(AF_INET, opp[0].data(), &opponent_addr.sin_addr);

						if (!config::DojoActAsServer)
						{
							host_addr.sin_port = htons((u16)std::stol(opp[1]));
							inet_pton(AF_INET, opp[0].data(), &host_addr.sin_addr);
						}

						SendPlayerName();
					}
				}
			}

			if (memcmp("SPECTATE", buffer, 8) == 0)
			{
				if (dojo.remaining_spectators > 0)
				{
					int port = atoi(buffer + 9);
					config::SpectatorIP = std::string(inet_ntoa(sender.sin_addr));
					config::SpectatorPort = std::to_string(port);
					SendSpectateOK(sender);
					dojo.remaining_spectators--;
				}
			}

			if (memcmp("NAME", buffer, 4) == 0)
			{
				settings.dojo.OpponentName = std::string(buffer + 5, strlen(buffer + 5));

				std::string buffer_str(buffer + 5, strlen(buffer + 5));
				auto tokens = stringfix::split(" ", buffer_str);

				settings.dojo.OpponentName = tokens[0];
				dojo.net_save_present = (bool)atoi(tokens[1].data());
				dojo.net_save_present = dojo.net_save_present && !config::IgnoreNetSave;

				opponent_addr = sender;

				// prepare for delay selection
				if (dojo.OpponentIP.empty())
					dojo.OpponentIP = std::string(inet_ntoa(opponent_addr.sin_addr));

				SendNameOK();

				dojo.isMatchReady = true;
				dojo.resume();
			}

			if (memcmp("OK NAME", buffer, 7) == 0)
			{
				name_acknowledged = true;
			}

			if (memcmp("REP", buffer, 3) == 0)
			{
				if (!last_sent.empty())
				{
					for (int i = 0; i < dojo.packets_per_frame; i++)
					{
						sendto(local_socket, (const char*)last_sent.data(), dojo.PayloadSize(), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
					}
				}
			}

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
			if (memcmp("START", buffer, 5) == 0)
			{
				if (config::GGPOEnable)
				{
					// close dojo matching functions and hand over to ggpo
					config::DojoEnable = false;
					return;
				}
				else
				{
					std::string buffer_str(buffer + 6, strlen(buffer + 6));
					auto tokens = stringfix::split(" ", buffer_str);

					int delay = atoi(tokens[0].data());
					int ppf = atoi(tokens[1].data());
					int nbf = atoi(tokens[2].data());
					std::string op = tokens[3];
					dojo.net_save_present = (bool)atoi(tokens[4].data());
					dojo.net_save_present = dojo.net_save_present && !config::IgnoreNetSave;

					settings.dojo.OpponentName = op;

					if (!dojo.session_started)
					{
						// adopt host delay and payload settings
						dojo.StartSession(delay, ppf, nbf);
					}
				}
			}

			if (memcmp("DISCONNECT", buffer, 10) == 0)
			{
				SendDisconnectOK();
				opponent_disconnected = true;
				dojo.disconnect_toggle = true;
			}

			if (memcmp("OK DISCONNECT", buffer, 13) == 0)
			{
				SendDisconnectOK();
				dojo.disconnect_toggle = true;
			}

			if (bytes_read == dojo.PayloadSize())
			{
				if (!dojo.isMatchReady && dojo.GetPlayer((u8 *)buffer) == dojo.opponent)
				{
					opponent_addr = sender;

					// prepare for delay selection
					if (dojo.hosting)
						dojo.OpponentIP = std::string(inet_ntoa(opponent_addr.sin_addr));

					dojo.isMatchReady = true;
					dojo.resume();
				}

				dojo.ClientReceiveAction((const char*)buffer);
			}
		}
	}

	if (!config::Receiving)
		SendDisconnect();
}

void UDPClient::CloseLocalSocket()
{
	CloseSocket(local_socket);
}

void UDPClient::ClientThread()
{
	Init(dojo.hosting);

	// if match code enabled
	// connect to matchmaking server
	if (config::EnableMatchCode)
	{
		if (!config::DojoActAsServer || (config::GGPOEnable && !config::ActAsServer))
			while (dojo.MatchCode.empty());
		ConnectMMServer();
	}

	ClientLoop();
	// close dojo udp client when ggpo session is activated
	if (!config::GGPOEnable)
		EndSession();
}

