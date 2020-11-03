#include "Ping.hpp"

// https://docs.microsoft.com/en-us/windows/desktop/api/icmpapi/nf-icmpapi-icmpsendecho
Ping::Ping() { open(); }
Ping::~Ping() { close(); }

int Ping::ping(const char* ipAddrString) const {
    if(!good()) {
        return Error_InvalidHandleValue;
    }

    IN_ADDR inAddr {};
    inet_pton(AF_INET, ipAddrString, &inAddr);

    static char sendData[] = "Ping";
    const auto replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData);
    char replyBuffer[replySize];
    const auto dwResult = IcmpSendEcho(
          hIcmpFile
        , inAddr.S_un.S_addr
        , sendData
        , sizeof(sendData)
        , nullptr
        , replyBuffer
        , sizeof(replyBuffer)
        , 1000
    );
    if(dwResult == 0) {
        return Error_IcmpSendEchoFailed;
    }

    auto* pEchoReply = reinterpret_cast<const ICMP_ECHO_REPLY*>(replyBuffer);
    return pEchoReply->RoundTripTime;
}

bool Ping::good() const {
    return INVALID_HANDLE_VALUE != hIcmpFile;
}

void Ping::open() {
     hIcmpFile = IcmpCreateFile();
}

void Ping::close() {
    if(good()) {
        IcmpCloseHandle(hIcmpFile);
    }
}

HANDLE hIcmpFile{ INVALID_HANDLE_VALUE };

