#pragma once

#include "BasicDef.h"

class UdpSocket
{
public:
    ~UdpSocket();

    int Bind(const class SocketAddress& _bind);

    int SendTo(const void* _data, int _size, const class SocketAddress& _to);

    int ReceiveFrom(void* _data, int _maxSize, class SocketAddress& _from);

    int SetNonBlockingMode(bool _isNonBlocking);

private:
    friend class SocketUtils;

    UdpSocket(SOCKET _socket);

    SOCKET mSocket;
};
