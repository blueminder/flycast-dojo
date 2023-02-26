#include <atomic>
#include <iostream>
#include <regex>
#include <string>

#include "network/net_platform.h"

class LobbyClient
{
private:
	int port;
	bool isLoopStarted;

	std::map<int, uint64_t> ping_send_ts;
	std::vector<uint64_t> ping_rtt;
	uint64_t avg_ping_ms;

	unsigned char to_send[256];

	sock_t local_socket = INVALID_SOCKET;
	void CloseSocket(sock_t& socket) const { closesocket(socket); socket = INVALID_SOCKET; }

	sock_t CreateAndBind(int port);
	bool CreateLocalSocket(int port);

	bool Init(bool hosting);
	void ClientLoop();

	unsigned long mix(unsigned long a, unsigned long b, unsigned long c);
	std::string random_hex_string(int length, int seed);

	void StartSessionOnReady(bool hosting);

	std::string longestCommonPrefix(std::vector<std::string>& str);
	std::string join_rand;

	std::set<std::string> join_msg_uids;
	std::set<std::string> joined_msg_uids;

public:
	LobbyClient();
	void ClientThread();

	int PingAddress(const char * ip_addr, int port, int add_to_seed);
	int PingAddress(sockaddr_in target_addr, int add_to_seed);
	uint64_t GetAvgPing(const char * ip_addr, int port);

	void SendMsg(std::string msg, std::string ip, int port);
	void SendMsg(std::string msg, sockaddr_in target_addr);

	void EndSession();
	void CloseLocalSocket();
};

