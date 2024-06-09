#pragma once

#include <chrono>
#include <iostream>
#include <thread>

#include "cfg/cfg.h"
#include "cfg/option.h"
#include "types.h"
#include "network/net_platform.h"

class RelayClient
{
public:
    void ClientThread();
    void ConnectRelayServer();

    bool disconnect_toggle;
    std::string target_hostname;
    bool start_game = false;

private:
    sock_t local_socket = INVALID_SOCKET;
    void CloseSocket(sock_t &socket) const
    {
        closesocket(socket);
        socket = INVALID_SOCKET;
    }

    sock_t CreateAndBind(int port);
    bool CreateLocalSocket(int port);

    bool Init();
    void ClientLoop();

    bool isLoopStarted;
    bool request_repeat;
};
