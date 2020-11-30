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

	long unix_timestamp();

	sock_t createAndBind(int port);
	bool createLocalSocket(int port);

	void SetHost(std::string host, int port);
	bool Init(bool hosting);

	void ClientLoop();
	//int Disconnect();

	bool disconnect_toggle;
	bool disconnect_complete;
	bool opponent_disconnected;

	void ClientThread();

	int SendData(std::string data);

	int PingAddress(const char * ip_addr, int port, int add_to_seed);
	int PingOpponent(int add_to_seed);

	int GetAvgPing(const char * ip_addr, int port);
	int GetOpponentAvgPing();


	void SendPlayerName();
	void SendNameOK();
	bool name_acknowledged;
	
	void StartSession();
	void EndSession();


	void SendDisconnect();
	void SendDisconnectOK();

	std::map<int, long> ping_send_ts;
	std::vector<long> ping_rtt;

	int avg_ping_ms;
};

