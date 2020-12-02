#include "DojoSession.hpp"

UDPClient::UDPClient()
{
	isLoopStarted = false;
	write_out = false;
	memset((void*)to_send, 0, 256);
	last_sent = "";
	disconnect_toggle = false;
	opponent_disconnected = false;
	name_acknowledged = false;
};

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
		ping_ss << "PING " << rnd_num_cmp;
		std::string to_send_ping = ping_ss.str();

		sendto(local_socket, (const char*)to_send_ping.data(), strlen(to_send_ping.data()), 0, (const struct sockaddr*)&target_addr, sizeof(target_addr));
		INFO_LOG(NETWORK, "Sent %s", to_send_ping.data());

		long current_timestamp = dojo.unix_timestamp();
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

int UDPClient::GetAvgPing(const char * ip_addr, int port)
{
	for (int i = 0; i < 5; i++)
	{
		PingAddress(ip_addr, port, i);
	}

	return avg_ping_ms;
}

int UDPClient::GetOpponentAvgPing()
{
	for (int i = 0; i < 5; i++)
	{
		PingAddress(opponent_addr, i);
	}

	return avg_ping_ms;
}

// for messages sent outside the game loop
void UDPClient::SendMsg(std::string msg, sockaddr_in target)
{
	for (int i = 0; i < settings.dojo.PacketsPerFrame; i++)
	{
		sendto(local_socket, (const char*)msg.data(), strlen(msg.data()), 0, (const struct sockaddr*)&target, sizeof(target));
	}

	INFO_LOG(NETWORK, "Message Sent: %s", msg.data());
}

void UDPClient::StartSession()
{
	std::stringstream start_ss("");
	start_ss << "START " << dojo.delay;
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
	SendMsg("NAME " + settings.dojo.PlayerName, host_addr);
}

void UDPClient::SendNameOK()
{
	SendMsg("OK NAME", opponent_addr);
}

void UDPClient::EndSession()
{
	gui_open_disconnected();
	dc_stop();
	CloseSocket(local_socket);
	WSACleanup();

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
		ERROR_LOG(NETWORK, "DojoSession UDP Server: bind() failed. errno=%d", get_last_error());
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
	if (!settings.dojo.Enable)
		return false;
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error());
		return false;
	}
#endif

	if (hosting)
	{
		return CreateLocalSocket(stoi(settings.dojo.ServerPort));
	}
	else
	{
		opponent_addr = host_addr;
		return CreateLocalSocket(0);
	}
}

void UDPClient::ClientLoop()
{
	isLoopStarted = true;

	while (!disconnect_toggle)
	{
		// if opponent detected, shoot packets at them
		if (opponent_addr.sin_port > 0 &&
			memcmp(to_send, last_sent.data(), dojo.PayloadSize()) != 0)
		{
			// send payload until morale improves
			for (int i = 0; i < settings.dojo.PacketsPerFrame; i++)
			{
				sendto(local_socket, (const char*)to_send, dojo.PayloadSize(), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
			}

			if (settings.dojo.Debug == DEBUG_SEND ||
				settings.dojo.Debug == DEBUG_SEND_RECV ||
				settings.dojo.Debug == DEBUG_ALL)
			{
				dojo.PrintFrameData("Sent", (u8 *)to_send);
			}

			last_sent = std::string(to_send, to_send + dojo.PayloadSize());

			if (dojo.client_input_authority)
				dojo.AddNetFrame((const char*)to_send);
		}

		struct sockaddr_in sender;
		socklen_t senderlen = sizeof(sender);
		char buffer[256];
		memset(buffer, 0, 256);
		int bytes_read = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderlen);
		if (bytes_read)
		{
			if (memcmp("NAME", buffer, 4) == 0)
			{
				settings.dojo.OpponentName = std::string(buffer + 5, strlen(buffer + 5));
				
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

			if (memcmp("START", buffer, 5) == 0)
			{
				int delay = atoi(buffer + 6);
				if (!dojo.session_started)
				{
					dojo.StartSession(delay);
				}
			}

			if (memcmp("DISCONNECT", buffer, 10) == 0)
			{
				SendDisconnectOK();
				opponent_disconnected = true;
				disconnect_toggle = true;
			}

			if (memcmp("OK DISCONNECT", buffer, 13) == 0)
			{
				SendDisconnectOK();
				disconnect_toggle = true;
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

	SendDisconnect();
}

void UDPClient::ClientThread()
{
	Init(dojo.hosting);
	ClientLoop();
	EndSession();
}

