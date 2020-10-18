#include <atomic>
#include <iostream>
#include <regex>
#include <string>

#include "../net_platform.h"

class UDPClient
{
private:
	int port;

	bool isStarted;
	bool isLoopStarted;

	std::atomic<bool> write_out;
	unsigned char to_send[256];

	std::string last_sent;

	sock_t local_socket = INVALID_SOCKET;

	sockaddr_in host_addr;
	sockaddr_in opponent_addr;

	void closeSocket(sock_t& socket) const { closesocket(socket); socket = INVALID_SOCKET; }

public:
	UDPClient();

	sock_t createAndBind(int port);
	bool createLocalSocket(int port);

	void SetHost(std::string host, int port);
	bool Init(bool hosting);

	void ClientLoop();
	//int Disconnect();

	bool disconnect_toggle;

	void ClientThread();

	int SendData(std::string data);
};
