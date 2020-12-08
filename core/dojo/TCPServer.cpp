#include "DojoSession.hpp"

TCPServer::TCPServer()
{
	isStarted = false;
	isLoopStarted = false;
}

bool TCPServer::Init()
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

	return CreateSocket();
}

bool TCPServer::CreateSocket()
{
	if (VALID(sock))
		return true;

	sock = CreateAndBind(port);
	if (!VALID(sock))
		return false;

	if (listen(sock, 5) < 0)
	{
		ERROR_LOG(NETWORK, "TCPServer: listen() failed. errno=%d", get_last_error());
		CloseSocket(sock);
		return false;
	}
	return true;
}

sock_t TCPServer::CreateAndBind(int port)
{
	sock_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!VALID(sock))
	{
		ERROR_LOG(NETWORK, "Cannot create socket");
		return sock;
	}
	int option = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option));

	struct sockaddr_in host_addr;
	memset(&host_addr, 0, sizeof(host_addr));

	host_addr.sin_family = AF_INET;
	host_addr.sin_port = htons((u16)port);

#ifdef _WIN32
	host_addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else
	host_addr.sin_addr.s_addr = INADDR_ANY;
#endif

	if (::bind(sock, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
	{
		ERROR_LOG(NETWORK, "TCP: bind() failed. errno=%d", get_last_error());
		CloseSocket(sock);
	}

	return sock;
}

void TCPServer::Listen()
{
	listen(sock, SOMAXCONN);

	// wait for connection
	int clientSize = sizeof(client_addr);

	client_sock = accept(sock, (sockaddr*)&client_addr, (socklen_t*)&clientSize);

	char host_name[NI_MAXHOST];		// client host name
	char service[NI_MAXSERV];	// service

	memset(host_name, 0, NI_MAXHOST);
	memset(service, 0, NI_MAXSERV);

	if (getnameinfo((sockaddr*)&client_addr, sizeof(client_addr), host_name, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)
	{
		INFO_LOG(NETWORK, "%s connected on port %s", host_name, service);
	}
	else
	{
		inet_ntop(AF_INET, &client_addr.sin_addr, host_name, NI_MAXHOST);
		INFO_LOG(NETWORK, "%s connected on port %d", host_name, ntohs(client_addr.sin_port));
	}

	CloseSocket(sock);
}

void TCPServer::ServerLoop()
{
	isLoopStarted = true;
	std::string last_sent_frame = "";

	char buf[4096];

	while (true)
	{
		memset(buf, 0, 4096);

		// Wait for client to send data
		int bytesReceived = recv(client_sock, buf, 4096, 0);
		if (bytesReceived == 0)
		{
			INFO_LOG(NETWORK, "client disconnected");
			break;
		}

		if (bytesReceived >= 12)
		{
			/*
			std::stringstream frame_ss(""); 
			frame_ss << "FRAME " << dojo.GetEffectiveFrameNumber((u8*)buf) << std::endl;
			std::string frame_s = frame_ss.str();
			*/

			std::string frame_s(buf, FRAME_SIZE);
			INFO_LOG(NETWORK, "SPECTATED %s", frame_s.data());
			
			dojo.AddNetFrame(frame_s.data());

			dojo.PrintFrameData("NETWORK", (u8*)frame_s.data());

			send(client_sock, frame_s.data(), frame_s.length(), 0);
		}


		if (bytesReceived == SOCKET_ERROR)
		{
			ERROR_LOG(NETWORK, "recv() error. quitting %d", get_last_error());
			break;
		}

	}
	
	CloseSocket(sock);

#ifdef _WIN32
	WSACleanup();
#endif
}

void TCPServer::ReceiverThread()
{
	port = stoi(settings.dojo.SpectatorPort);

	isStarted = true;

	Init();
	CreateSocket();
	Listen();
	ServerLoop();
}

