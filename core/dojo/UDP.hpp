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

	std::map<int, uint64_t> ping_send_ts;
	std::vector<uint64_t> ping_rtt;
	uint64_t avg_ping_ms;

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

	void SendSpectate();
	void SendSpectateOK(sockaddr_in target_addr);

	void StartSession();
	void EndSession();

	void ConnectMMServer();

	void SetHost(std::string host, int port);

	std::string random_hex_string(int length, int seed);

	int PingAddress(const char * ip_addr, int port, int add_to_seed);
	int PingAddress(sockaddr_in target_addr, int add_to_seed);

	uint64_t GetAvgPing(const char * ip_addr, int port);
	uint64_t GetOpponentAvgPing(int num_requests = 5);

	int SendData(std::string data);

	void SendMsg(std::string msg, sockaddr_in target);
	void SendPlayerName();
	void SendNameOK();

	void SendDisconnect();
	void SendDisconnectOK();

	bool opponent_disconnected;

	bool name_acknowledged;
	bool request_repeat;

	void CloseLocalSocket();
};

