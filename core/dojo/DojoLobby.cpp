#include "DojoSession.hpp"

DojoLobby::DojoLobby()
{
}

void DojoLobby::BeaconThread()
{
    beacon("224.0.0.1", 7776, 5);
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
    while (dojo.host_status < 4) {
        const char* message;

        switch (dojo.host_status)
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
        message_ss << settings.dojo.ServerPort << "__" << status << "__" << current_game << "__" << settings.dojo.PlayerName;
        if (dojo.host_status == 2 || dojo.host_status == 3)
        {
            message_ss << " vs " << settings.dojo.OpponentName << "__";
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
            memcpy(s, "Unknown AF", strlen("Unknown AF") + 1);
            //strncpy_s(s, strlen("Unknown AF"), "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

int DojoLobby::beacon(char* group, int port, int delay_secs)
{
    beacon_sock = Init();
    sockaddr_in addr = SetDestination(group, port);
    BeaconLoop(addr, delay_secs);
    CloseSocket(beacon_sock);

    return 0;
}

void DojoLobby::ListenerAction(asio::ip::udp::endpoint beacon_endpoint, char* msgbuf, int length)
{
        std::string ip_str = beacon_endpoint.address().to_string();
        std::string port_str = std::to_string(beacon_endpoint.port());

        std::stringstream bi;
        bi << ip_str << ":" << port_str;
        std::string beacon_id = bi.str();
        
        std::stringstream bm;
        bm << ip_str <<  "__" << msgbuf;
        
        if (dojo.host_status == 0)
        {
            if (active_beacons.count(beacon_id) == 0)
            {
                active_beacons.insert(std::pair<std::string, std::string>(beacon_id, bm.str()));
                
                //int avg_ping_ms = dojo.client.GetAvgPing(beacon_id.c_str(), addr.sin_port);
                int avg_ping_ms = 0;
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
            
            last_seen[beacon_id] = dojo.unix_timestamp();
        }
}

Listener::Listener(asio::io_context& io_context,
		const asio::ip::address& listen_address,
		const asio::ip::address& multicast_address)
	: socket_(io_context)
{
	// create socket so that multiple may be bound to the same address
	asio::ip::udp::endpoint listen_endpoint(
		listen_address, 7776);
	socket_.open(listen_endpoint.protocol());
	socket_.set_option(asio::ip::udp::socket::reuse_address(true));
	socket_.bind(listen_endpoint);

	// join multicast group
	socket_.set_option(
		asio::ip::multicast::join_group(multicast_address));

	do_receive();
}

void Listener::do_receive()
{
	if (dojo.host_status != 0 || dc_is_running())
		return;

	socket_.async_receive_from(
		asio::buffer(data_), beacon_endpoint_,
		[this](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				//std::cout.write(data_.data(), length);
				//std::cout << std::endl;

				dojo.presence.ListenerAction(beacon_endpoint_, data_.data(), length);

				do_receive();
			}
		});
}

void DojoLobby::ListenerThread()
{
    try
    {
        asio::io_context io_context;
        Listener l(io_context,
            asio::ip::make_address("0.0.0.0"),
            asio::ip::make_address("224.0.0.1"));

        dojo.lobby_active = true;

        io_context.run();
    }
    catch (...) {}
}


