#pragma once

#include <memory>
#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

using SocketAddressPtr = std::shared_ptr<class SocketAddress>;

using TcpSocketPtr = std::shared_ptr<class TcpSocket>;
using UdpSocketPtr = std::shared_ptr<class UdpSocket>;

enum class ADDRFAM
{
    INET = AF_INET,
    INET6 = AF_INET6
};
