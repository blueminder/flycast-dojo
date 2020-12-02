#include <atomic>
#include <iostream>
#include <regex>
#include <string>

#include "network/net_platform.h"

class UDPClient
{
private:
	int port;
	bool isLoopStarted;

	std::map<int, long> ping_send_ts;
	std::vector<long> ping_rtt;
	int avg_ping_ms;

	std::atomic<bool> write_out;
	unsigned char to_send[256];

	std::string last_sent;

	sockaddr_in host_addr;
	sockaddr_in opponent_addr;

	sock_t local_socket = INVALID_SOCKET;
	void CloseSocket(sock_t& socket) const { closesocket(socket); socket = INVALID_SOCKET; }

	sock_t CreateAndBind(int port);
	bool CreateLocalSocket(int port);

	bool Init(bool hosting);
	void ClientLoop();

public:
	UDPClient();
	void ClientThread();

	void StartSession();
	void EndSession();

	void SetHost(std::string host, int port);

	int PingAddress(const char * ip_addr, int port, int add_to_seed);
	int PingAddress(sockaddr_in target_addr, int add_to_seed);

	int GetAvgPing(const char * ip_addr, int port);
	int GetOpponentAvgPing();

	int SendData(std::string data);
	void SendMsg(std::string msg, sockaddr_in target);
	void SendPlayerName();
	void SendNameOK();

	void SendDisconnect();
	void SendDisconnectOK();

	bool disconnect_toggle;
	bool opponent_disconnected;

	bool name_acknowledged;
};

