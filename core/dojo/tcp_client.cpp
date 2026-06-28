#include "dojo.h"

TcpClient::TcpClient()
{
	isLoopStarted = false;
	endSession = false;
	transmitter_started = false;
	receiver_started = false;
}

bool TcpClient::Init()
{
	isLoopStarted = false;
	endSession = false;

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

bool TcpClient::CreateSocket()
{
	if (VALID(sock))
		return true;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	return true;
}

int hostname_to_ip(char *hostname, char *ip)
{
	struct hostent *he;
	struct in_addr **addr_list;
	int i;

	if ((he = gethostbyname(hostname)) == NULL)
	{
		// get the host info
		// herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **)he->h_addr_list;

	for (i = 0; addr_list[i] != NULL; i++)
	{
		// Return the first one;
		strcpy(ip, inet_ntoa(*addr_list[i]));
		return 0;
	}

	return 1;
}

void TcpClient::Connect()
{
	char ip[100];
	hostname_to_ip(host.data(), ip);
	std::string host_ip = std::string(ip, strlen(ip));

	memset(&host_addr, 0, sizeof(host_addr));

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u16)port);
	inet_pton(AF_INET, host_ip.data(), &host_addr.sin_addr);

	if (::connect(sock, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
	{
		ERROR_LOG(NETWORK, "Socket connect failed");
		CloseSocket(sock);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void TcpClient::Disconnect()
{
	CloseSocket(sock);
#ifdef _WIN32
	WSACleanup();
#endif
}

void TcpClient::Stop()
{
	endSession = true;
}

void TcpClient::TransmissionLoop()
{
	isLoopStarted = true;
	std::string last_sent_msg = "";

	char buf[4096];

	while (!endSession)
	{
		if (!outgoing_msgs.empty())
		{
			last_sent_msg = outgoing_msgs.front();
			memcpy((void *)to_send, last_sent_msg.data(), last_sent_msg.size());

			int sendResult = send(sock, (const char *)to_send, last_sent_msg.size(), 0);
			if (VALID(sendResult))
			{
				outgoing_msgs.pop();
				memset(to_send, 0, 4096);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void TcpClient::TransmissionThread()
{
	host = config::SpectatorIP;
	port = stoi(config::SpectatorPort);

	transmitter_started = true;

	Init();
	Connect();
	TransmissionLoop();
	Disconnect();

	transmitter_started = false;
}

void TcpClient::ReceivingInit()
{
	MessageWriter spectate_request;

	spectate_request.AppendHeader(1, SPECTATE_REQUEST);

	spectate_request.AppendString(cfgLoadStr("dojo", "Quark", ""));
	spectate_request.AppendString(cfgLoadStr("dojo", "SpectateKey", ""));
	spectate_request.AppendString(dojo.game_name);

	std::vector<unsigned char> message = spectate_request.Msg();

	memcpy((void *)to_send, message.data(), message.size());
	int sendResult = send(sock, (const char *)to_send, message.size(), 0);
	if (VALID(sendResult))
	{
		memset(to_send, 0, 256);
	}
}

void TcpClient::ReceivingLoop()
{
	isLoopStarted = true;
	char buf[4096];
	memset(buf, 0, 4096);
	int offset = 0;

	while (!endSession)
	{
		int headerBytesReceived = 0;
		while (headerBytesReceived < HEADER_LEN && !endSession)
			headerBytesReceived = recv(sock, buf, HEADER_LEN, MSG_PEEK);

		if (endSession)
			return;

		if (headerBytesReceived == HEADER_LEN)
		{
			headerBytesReceived = recv(sock, buf, HEADER_LEN, 0);
			u32 body_size = HeaderReader::GetSize((u8 *)buf);
			u32 seq = HeaderReader::GetSeq((u8 *)buf);
			u32 cmd = HeaderReader::GetCmd((u8 *)buf);

			memset(buf, 0, 4096);

			int bodyBytesReceived = 0;
			while (bodyBytesReceived < body_size && !endSession)
				bodyBytesReceived = recv(sock, buf, body_size, MSG_PEEK);

			if (endSession)
				return;

			if (bodyBytesReceived == body_size)
			{
				bodyBytesReceived = recv(sock, buf, body_size, 0);
				offset = 0;
				dojo.ProcessBody(cmd, body_size, (const char *)buf, &offset);
				memset(buf, 0, 4096);
			}
		}
	}
}

void TcpClient::ReceivingThread()
{
	host = cfgLoadStr("dojo", "SpectatorIP", "");
	port = stoi(cfgLoadStr("dojo", "SpectatorPort", ""));

	receiver_started = true;

	Init();
	Connect();
	ReceivingInit();
	ReceivingLoop();
	Disconnect();

	receiver_started = false;
}
