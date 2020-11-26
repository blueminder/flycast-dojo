#include "MapleNet.hpp"

TCPClient::TCPClient()
{
	isStarted = false;
	isLoopStarted = false;
	endSession = false;
}

bool TCPClient::Init()
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

	return CreateSocket();
}

bool TCPClient::CreateSocket()
{
	if (VALID(sock))
		return true;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	return true;
}

void TCPClient::Connect()
{

	struct sockaddr_in host_addr;
	memset(&host_addr, 0, sizeof(host_addr));

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, host.data(), &host_addr.sin_addr);

	if (::connect(sock, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
	{
		ERROR_LOG(NETWORK, "Socket connect failed");
		CloseSocket(sock);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void TCPClient::ClientLoop()
{
	isLoopStarted = true;
	std::string last_sent_frame = "";

	char buf[4096];

	do
	{
		if (!transmission_frames.empty())
		{
			last_sent_frame = transmission_frames.front();
			memcpy((void*)to_send, last_sent_frame.data(), FRAME_SIZE);

			int sendResult = sendto(sock, (const char*)to_send, FRAME_SIZE, 0, (const struct sockaddr*)&host_addr, sizeof(host_addr));
			if (sendResult != SOCKET_ERROR)
			{
				// Wait for response
				memset(buf, 0, 4096);
				int bytesReceived = recv(sock, buf, 4096, 0);
				if (bytesReceived > 0)
				{
					std::string received = std::string(buf, 0, bytesReceived);
					INFO_LOG(NETWORK, "SPECTATED %s", received.data());
					//maplenet.PrintFrameData("SPECTATED ", (u8*)received.data());
					//if (received == transmission_frames.front())
					//{
					transmission_frames.pop();
					memset(to_send, 0, 256);
					//}

					// Echo response to console
				}
			}
		}
	} while (true);

	CloseSocket(sock);

#ifdef _WIN32
	WSACleanup();
#endif
}

void TCPClient::TransmissionThread()
{
	host = settings.maplenet.SpectatorIP;
	port = stoi(settings.maplenet.SpectatorPort);

	isStarted = true;

	Init();
	Connect();
	ClientLoop();
}

