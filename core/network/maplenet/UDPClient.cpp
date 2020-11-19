#include "MapleNet.hpp"


UDPClient::UDPClient()
{
	isStarted = false;
	isLoopStarted = false;
	write_out = false;
	memset((void*)to_send, 0, 256);
	last_sent = "";
	disconnect_toggle = false;
};

void UDPClient::SetHost(std::string host, int port)
{
	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, host.data(), &host_addr.sin_addr);
}

int UDPClient::SendData(std::string data)
{
	// sets current send buffer
	write_out = true;
	memcpy((void*)to_send, data.data(), maplenet.PayloadSize());
	write_out = false;

	if (maplenet.client_input_authority)
		maplenet.AddNetFrame((const char*)to_send);

	return 0;
}

// udp ping, seeds with random number
int UDPClient::PingOpponent()
{
	srand(time(NULL));
	int rnd_num_cmp = rand() * 1000 + 1;

	long current_timestamp = unix_timestamp(); 
	ping_send_ts.emplace(rnd_num_cmp, current_timestamp);
	
	return rnd_num_cmp;
}

sock_t UDPClient::createAndBind(int port)
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
		ERROR_LOG(NETWORK, "MapleNet UDP Server: bind() failed. errno=%d", get_last_error());
		closeSocket(sock);
	}
	else
		set_non_blocking(sock);

	return sock;
}

bool UDPClient::createLocalSocket(int port)
{
	if (!VALID(local_socket))
		local_socket = createAndBind(port);

	return VALID(local_socket);
}

bool UDPClient::Init(bool hosting)
{
	if (!settings.maplenet.Enable)
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
		return createLocalSocket(stoi(settings.maplenet.ServerPort));
	}
	else
	{
		opponent_addr = host_addr;
		return createLocalSocket(0);
	}
		
}

long UDPClient::unix_timestamp()
{
    time_t t = time(0);
    long int now = static_cast<long int> (t);
    return now;
}

void UDPClient::ClientLoop()
{
	isLoopStarted = true;

	while (true)
	{
		while (true)
		{
			if (disconnect_toggle)
			{
				char disconnect_packet[2];
				disconnect_packet[0] = 0xFF;
				disconnect_packet[1] = 0xFF;
				for (int i = 0; i < settings.maplenet.PacketsPerFrame; i++)
				{
					sendto(local_socket, (const char*)disconnect_packet, 2, 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				}
			}

			// if match has not started, send packet to inform host who the opponent is
			if (maplenet.FrameNumber > 0 && !maplenet.hosting)
			{
				if (!maplenet.isMatchStarted)
				{
					sendto(local_socket, (const char*)to_send, maplenet.PayloadSize(), 0, (const struct sockaddr*)&host_addr, sizeof(host_addr));
				}
			}

			// if opponent detected, shoot packets at them
			if (opponent_addr.sin_port > 0 &&
				memcmp(to_send, last_sent.data(), maplenet.PayloadSize()) != 0)
			{
				// send payload until morale improves
				for (int i = 0; i < settings.maplenet.PacketsPerFrame; i++)
				{
					sendto(local_socket, (const char*)to_send, maplenet.PayloadSize(), 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				}

				if (settings.maplenet.Debug == DEBUG_SEND ||
					settings.maplenet.Debug == DEBUG_SEND_RECV ||
					settings.maplenet.Debug == DEBUG_ALL)
				{
					maplenet.PrintFrameData("Sent", (u8 *)to_send);
				}

				last_sent = std::string(to_send, to_send + maplenet.PayloadSize());

				if (maplenet.client_input_authority)
					maplenet.AddNetFrame((const char*)to_send);
			}

			struct sockaddr_in sender;
			socklen_t senderlen = sizeof(sender);
			char buffer[256];
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
					//srand (time(NULL));
					//int rnd_num_cmp = rand() * 1000 + 1;

					int rnd_num_cmp = atoi(buffer + 5);
					long ret_timestamp = unix_timestamp();
					long rtt;
					if (ping_send_ts.at(rnd_num_cmp))
					{
						ping_rtt.push_back(ret_timestamp - ping_send_ts[rnd_num_cmp]);
						ping_send_ts.erase(rnd_num_cmp);
					}
				}

				if (maplenet.GetPlayer((u8*)buffer) == 0xFF && maplenet.GetDelay((u8*)buffer) == 0xFF)
					disconnect_toggle = true;

				if (bytes_read == maplenet.PayloadSize())
				{
					if (!maplenet.isMatchReady && maplenet.GetPlayer((u8 *)buffer) == maplenet.opponent)
					{
						opponent_addr = sender;

						// prepare for delay selection
						if (maplenet.hosting)
							maplenet.OpponentIP = std::string(inet_ntoa(opponent_addr.sin_addr));

						maplenet.isMatchReady = true;
						maplenet.resume();
					}

					maplenet.ClientReceiveAction((const char*)buffer);
				}
			}

			//maplenet.ClientLoopAction();
			//wait_seconds(0.006f);
		}
	}
}

void UDPClient::ClientThread()
{
	Init(maplenet.hosting);
	ClientLoop();
}
