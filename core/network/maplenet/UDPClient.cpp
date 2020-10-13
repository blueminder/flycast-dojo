#include "MapleNet.hpp"

#include "../net_platform.h"

UDPClient::UDPClient()
{
	isStarted = false;
	isLoopStarted = false;
	write_out = false;
	memset((void*)to_send, 0, 256);
	last_sent = "";
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
	memcpy((void*)to_send, data.data(), PAYLOAD_SIZE);
	write_out = false;

	if (maplenet.client_input_authority)
		maplenet.AddNetFrame((const char*)to_send);

	return 0;
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

void UDPClient::ClientLoop()
{
	isLoopStarted = true;

	while (true)
	{
		// if match has not started, send packet to inform host who the opponent is
		if (maplenet.FrameNumber > 0 && !maplenet.hosting)
		{
			if (!maplenet.isMatchStarted)
			{
				//socket.Send(host, to_send, PAYLOAD_SIZE);
				sendto(local_socket, (const char*)to_send, PAYLOAD_SIZE, 0, (const struct sockaddr*)&host_addr, sizeof(host_addr));
			}
		}

		while (true)
		{
			// if opponent detected, shoot packets at them
			if (opponent_addr.sin_port > 0 &&
				memcmp(to_send, last_sent.data(), PAYLOAD_SIZE) != 0)
			{
				// send payload until morale improves
				for (int i = 0; i < 5; i++)
				{
					//socket.Send(opponent, to_send, PAYLOAD_SIZE);
					sendto(local_socket, (const char*)to_send, PAYLOAD_SIZE, 0, (const struct sockaddr*)&opponent_addr, sizeof(opponent_addr));
				}

				if (settings.maplenet.Debug == DEBUG_SEND ||
					settings.maplenet.Debug == DEBUG_SEND_RECV ||
					settings.maplenet.Debug == DEBUG_ALL)
				{
					maplenet.PrintFrameData("Sent", (u8 *)to_send);
				}

				last_sent = std::string(to_send, to_send + PAYLOAD_SIZE);

				if (maplenet.client_input_authority)
					maplenet.AddNetFrame((const char*)to_send);
			}

			struct sockaddr_in sender;
			socklen_t senderlen = sizeof(sender);
			//Address sender;
			char buffer[256];
			//int bytes_read = socket.Receive(sender, buffer, sizeof(buffer));
			int bytes_read = recvfrom(local_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderlen);
			if (bytes_read)
			{
				if (bytes_read == PAYLOAD_SIZE)
				{
					if (!maplenet.isMatchReady && maplenet.GetPlayer((u8 *)buffer) == maplenet.opponent)
					{
						opponent_addr = sender;
						maplenet.isMatchReady = true;
						maplenet.isMatchStarted = true;
						maplenet.resume();
					}

					maplenet.ClientReceiveAction((const char*)buffer);
				}
			}

			maplenet.ClientLoopAction();
			//wait_seconds(0.006f);
		}
	}
}

void UDPClient::ClientThread()
{
	Init(maplenet.hosting);
	ClientLoop();
}
