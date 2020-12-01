#pragma once

#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#ifdef _WIN32
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

#include <map>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stdclass.h"

#define MSGBUFSIZE 256

class LobbyPresence
{
public:
    void BeaconThread();
    void ListenerThread();

    std::map<std::string, std::string> active_beacons;
    std::map<std::string, int> active_beacon_ping;
    std::map<std::string, long> last_seen;

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
