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


#define MSGBUFSIZE 256

class LobbyPresence
{
public:
    void BeaconThread();
    void ListenerThread();

    std::map<std::string, std::string> active_beacons;

private:
    int beacon(char* group, int port, int delay_secs);
    int listener(char* group, int port);

};
