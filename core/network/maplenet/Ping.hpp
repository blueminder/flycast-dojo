// https://docs.microsoft.com/en-us/windows/desktop/api/icmpapi/nf-icmpapi-icmpsendecho
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <stdio.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

class Ping
{
public:
    const int Error_InvalidHandleValue = -1;
    const int Error_IcmpSendEchoFailed = -2;

    Ping();
    ~Ping();

    int ping(const char* ipAddrString) const;

protected:
    void open();
    void close();
    bool good() const;

    HANDLE hIcmpFile;
};

