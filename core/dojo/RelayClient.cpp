#include "DojoSession.hpp"
#include "deps/Base64.h"

void split(std::string const &str, const char delim,
		   std::vector<std::string> &out)
{
	size_t start;
	size_t end = 0;

	while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
	{
		end = str.find(delim, start);
		out.push_back(str.substr(start, end - start));
	}
}

// connects to relay server
void RelayClient::ConnectRelayServer()
{
	struct hostent *mm_host;
	target_hostname = config::NetworkServer.get();
	mm_host = gethostbyname(target_hostname.data());

	sockaddr_in mms_addr;
	mms_addr.sin_family = AF_INET;
	mms_addr.sin_port = htons((u16)config::GGPORemotePort.get());
	memcpy(&mms_addr.sin_addr, mm_host->h_addr_list[0], mm_host->h_length);
	// inet_pton(AF_INET, config::NetworkServer.data(), &mms_addr.sin_addr);
	std::string ip_address = inet_ntoa(*((in_addr *)mm_host->h_addr));
	config::NetworkServer.set(ip_address);

	std::string b64_game_name = macaron::Base64::Encode(dojo.game_name.data());

	std::string mm_msg;
	if (config::ActAsServer)
		mm_msg = "host|" + b64_game_name;
	else
	{
		std::string relay_key = cfgLoadStr("dojo", "RelayKey", "");

		while (relay_key.empty() && !disconnect_toggle)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (disconnect_toggle)
			return;

		mm_msg = "join|" + relay_key;
	}

	sendto(local_socket, (const char *)mm_msg.data(), strlen(mm_msg.data()), 0, (const struct sockaddr *)&mms_addr, sizeof(mms_addr));

	INFO_LOG(NETWORK, "Connecting to Relay");
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
		ERROR_LOG(NETWORK, "Dojo Relay Client: bind() failed. errno=%d", get_last_error_n());
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
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		ERROR_LOG(NETWORK, "WSAStartup failed. errno=%d", get_last_error_n());
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
			if (memcmp("NOKEY", buffer, 5) == 0)
			{
				std::cout << "NOKEY" << std::endl;
				std::string received = std::string(buffer, 6);
				cfgSetVirtual("dojo", "RelayKey", received);
				disconnect_toggle = true;
			}
			else if (memcmp("START", buffer, 5) == 0)
			{
				start_game = true;
				disconnect_toggle = true;
			}
			else if (bytes_read == 6)
			{
				std::string received = std::string(buffer, 6);
				cfgSetVirtual("dojo", "RelayKey", received);
				std::cout << "RECEIVED RELAY KEY " << received << std::endl;
				start_game = true;
				disconnect_toggle = true;
			}
		}
	}
}

void RelayClient::ClientThread()
{
	Init();
	ConnectRelayServer();
	ClientLoop();
	CloseSocket(local_socket);
}
