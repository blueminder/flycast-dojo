#include "DojoSession.hpp"

void DojoLobby::BeaconThread()
{
    beacon("224.0.0.1", 7776, 5);
}

void DojoLobby::ListenerThread()
{
    listener("224.0.0.1", 7776);
}

int DojoLobby::Init()
{
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

    return fd;
}

void DojoLobby::CloseSocket(int sock)
{
    closesocket(sock);

#ifdef _WIN32
    // shut down winsock cleanly
    //WSACleanup();
#endif
}

int DojoLobby::BeaconLoop(sockaddr_in addr, int delay_secs)
{
    std::string current_game = get_file_basename(settings.imgread.ImagePath);
    current_game = current_game.substr(current_game.find_last_of("/\\") + 1);

    std::string status;
    std::string data;

    // sendto() destination
    while (maplenet.host_status < 4) {
        const char* message;

        switch (maplenet.host_status)
        {
        case 0:
            status = "Idle";
            break;
        case 1:
            status = "Hosting, Waiting";
            break;
        case 2:
            status = "Hosting, Starting";
            break;
        case 3:
            status = "Hosting, Playing";
            break;
        case 4:
            status = "Guest, Connecting";
            break;
        case 5:
            status = "Guest, Playing";
            break;
        }

		std::stringstream message_ss("");
        message_ss << settings.maplenet.ServerPort << "__" << status << "__" << current_game << "__" << settings.maplenet.PlayerName;
        if (maplenet.host_status == 2 || maplenet.host_status == 3)
        {
            message_ss << " vs " << settings.maplenet.OpponentName << "__";
        }
        else
        {
            message_ss << "__";
        }
        std::string message_str = message_ss.str();
        message = message_str.data();

        char ch = 0;
        int nbytes = sendto(
            beacon_sock,
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

    return 0;
}

sockaddr_in DojoLobby::SetDestination(char* group, short port)
{
    // set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (group == 0)
        addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender
    else
        addr.sin_addr.s_addr = inet_addr(group);
    addr.sin_port = htons(port);

    return addr;
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

int DojoLobby::ListenerLoop(sockaddr_in addr)
{
    while (maplenet.host_status == 0 && !dc_is_running())
    {
        char msgbuf[MSGBUFSIZE];
        char ip_str[128];

        int addrlen = sizeof(addr);
        int nbytes = recvfrom(
            listener_sock,
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
        INFO_LOG(NETWORK, "%s %u", ip_str, addr.sin_port);
        INFO_LOG(NETWORK, msgbuf);
        
        std::stringstream bi;
        bi << ip_str << ":" << std::to_string(addr.sin_port);
        std::string beacon_id = bi.str();
        
        std::stringstream bm;
        bm << ip_str <<  "__" << msgbuf;
        
        if (maplenet.host_status == 0)
        {
            if (active_beacons.count(beacon_id) == 0)
            {
                active_beacons.insert(std::pair<std::string, std::string>(beacon_id, bm.str()));
                
                int avg_ping_ms = maplenet.client.GetAvgPing(beacon_id.c_str(), addr.sin_port);
                active_beacon_ping.insert(std::pair<std::string, int>(beacon_id, avg_ping_ms));
            }
            else
            {
                try {
                    if (active_beacons.at(beacon_id) != bm.str())
                        active_beacons.at(beacon_id) = bm.str();
                    }
                catch (...) {};
            }
            
            last_seen[beacon_id] = maplenet.unix_timestamp();
        }
    }

    return 0;
}

int DojoLobby::beacon(char* group, int port, int delay_secs)
{
    beacon_sock = Init();
    sockaddr_in addr = SetDestination(group, port);
    BeaconLoop(addr, delay_secs);
    CloseSocket(beacon_sock);

    return 0;
}

int DojoLobby::listener(char* group, int port)
{
    listener_sock = Init();

    // allow multiple sockets to use the same port number
    u_int yes = 1;
    if (
        setsockopt(
            listener_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)
        ) < 0
    ){
       perror("Reusing ADDR failed");
       return 1;
    }

    sockaddr_in addr = SetDestination(0, port);

    // bind to receive address
    if (bind(listener_sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    // use setsockopt() to request that the kernel join a multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(group);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            listener_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)
        ) < 0
    ){
        perror("setsockopt");
        return 1;
    }

    ListenerLoop(addr);
    CloseSocket(listener_sock);

    return 0;
}

