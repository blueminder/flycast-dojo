#pragma once

#ifdef _WIN32
    #pragma comment(lib, "Ws2_32.lib")
    #define _WINSOCK_DEPRECATED_NO_WARNINGS 1

    #include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
    #include <Ws2tcpip.h> // needed for ip_mreq definition for multicast
    #include <Windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <time.h>
#endif

#define SOCKET_ERROR -1

#include <map>

#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "stdclass.h"
#include "cfg/option.h"

#define MSGBUFSIZE 256

#include "LobbyClient.hpp"

constexpr int MAX_PLAYERS = 2;

typedef struct m_player {
    std::string name;
    std::string ip;
    int listen_port;
    int port_num;
    int ggpo_port;
} Player;

class DojoLobby
{
public:
    void BeaconThread();
    void ListenerThread();

    LobbyClient client;

    std::map<std::string, std::string> active_beacons;
    std::map<std::string, int> active_beacon_ping;
    std::map<std::string, uint64_t> last_seen;

    int CancelHost();

    int SendMsg(const char* ip, int port, const char* msg);
    int SendJoin(const char* ip);
    int SendConnected(const char* ip, const char* joined_name, const char* joined_ip);
    int SendGameStart(const char* ip);
    int CloseLobby();

    bool lobby_disconnect_toggle = false;
    bool hosting_lobby = false;
    int player_count = 0;

    std::vector<std::string> player_names = { "", "", "", "" };
    std::vector<std::string> player_ips = { "", "", "", "" };
    std::vector<int> player_ggpo_ports = { 0, 0, 0, 0 };

    std::set<std::string> targets;
    std::vector<Player> players;

private:
    int beacon(char* group, int port, int delay_secs);
    int listener(char* group, int port);

    int listener_sock;
    int beacon_sock;

    int Init();

    sockaddr_in SetDestination(char* group, short port);

    int BeaconLoop(sockaddr_in addr, int delay_secs);
    std::string ConstructMsg();

    int ListenerLoop(sockaddr_in addr);
    void CloseSocket(int sock);

};
