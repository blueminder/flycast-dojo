#include "MapleNet.hpp"

void LobbyPresence::BeaconThread()
{
    beacon("238.255.255.250", 1900, 1);
}

void LobbyPresence::ListenerThread()
{
    listener("238.255.255.250", 1900);
}

int LobbyPresence::beacon(char* group, int port, int delay_secs)
{
    std::string current_game = settings.imgread.ImagePath;
    current_game = current_game.substr(current_game.find_last_of("/\\") + 1);

    std::string status;
    std::string data;

#ifdef _WIN32
    // initialize winsock
    WSADATA wsaData;
    if (WSAStartup(0x0101, &wsaData)) {
        perror("WSAStartup");
        return 1;
    }
#endif

    // create UDP socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    // set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(group);
    addr.sin_port = htons(port);

    // sendto() destination
    while (1) {
        const char* message;

        switch (maplenet.host_status)
        {
        case 0:
            status = "IDLE__" + settings.maplenet.ServerPort;
            break;
        case 1:
            status = "HOST_WAIT__" + settings.maplenet.ServerPort;
            break;
        case 2:
            status = "HOST_PLAYING";
            break;
        case 3:
            status = "GUEST_CONNECTING";
            break;
        case 4:
            status = "HOST_PLAYING";
            break;
        }

        std::string message_str = status + "__" + current_game;
        message = message_str.c_str();

        char ch = 0;
        int nbytes = sendto(
            fd,
            message,
            strlen(message),
            0,
            (struct sockaddr*) &addr,
            sizeof(addr)
        );
        if (nbytes < 0) {
            perror("sendto");
            return 1;
        }

     #ifdef _WIN32
          Sleep(delay_secs * 1000); // windows sleep in ms 
     #else
          sleep(delay_secs); // unix sleep in seconds
     #endif
     }

#ifdef _WIN32
    // shut down winsock cleanly
    // never actually reached
    WSACleanup();
#endif
}

char* get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy_s(s, strlen("Unknown AF"), "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

int LobbyPresence::listener(char* group, int port)
{
#ifdef _WIN32
    // initialize winsock 
    WSADATA wsaData;
    if (WSAStartup(0x0101, &wsaData)) {
        perror("WSAStartup");
        return 1;
    }
#endif

    // create what looks like an ordinary UDP socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    // allow multiple sockets to use the same PORT number
    u_int yes = 1;
    if (
        setsockopt(
            fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)
        ) < 0
    ){
       perror("Reusing ADDR failed");
       return 1;
    }

    // set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    addr.sin_port = htons(port);

    // bind to receive address
    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)
        ) < 0
    ){
        perror("setsockopt");
        return 1;
    }

    // now just enter a read-print loop
    while (1) {
        char msgbuf[MSGBUFSIZE];
        char ip_str[128];

        int addrlen = sizeof(addr);
        int nbytes = recvfrom(
            fd,
            msgbuf,
            MSGBUFSIZE,
            0,
            (struct sockaddr *) &addr,
            &addrlen
        );
        if (nbytes < 0) {
            perror("recvfrom");
            return 1;
        }
        msgbuf[nbytes] = '\0';

        get_ip_str((struct sockaddr *) &addr, ip_str, 128);
        int sport = getpeername(fd, &addr, &addrlen);
        printf("%s %d ", ip_str, addr.sin_port);
        puts(msgbuf);

        std::string beacon_id = std::string(ip_str, strlen(ip_str)) + ":" + std::to_string(sport);
        active_beacons[beacon_id] = std::string(msgbuf, strlen(msgbuf));
     }

#ifdef _WIN32
    // shut down winsock cleanly
    // never actually reached
    WSACleanup();
#endif

    return 0;
}
