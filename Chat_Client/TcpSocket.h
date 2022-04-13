#pragma once

#include "BasicDef.h"

class TcpSocket
{
public:
    ~TcpSocket();

    int Connect(const SocketAddress& _addr);

    int Bind(const SocketAddress& _addr);

    int Listen(int _backLog = 32);

    TcpSocketPtr Accept(SocketAddress& _from);

    int32_t Send(const void* _data, size_t _size);

    int32_t Receive(void* _buffer, size_t _size);

private:
    friend class SocketUtils;

    TcpSocket(SOCKET _socket);

    SOCKET mSocket;
};
