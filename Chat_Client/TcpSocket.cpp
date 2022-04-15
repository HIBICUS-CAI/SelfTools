#include "TcpSocket.h"
#include "SocketUtils.h"
#include "SocketAddress.h"

TcpSocket::TcpSocket(SOCKET _socket) :mSocket(_socket) {}

TcpSocket::~TcpSocket()
{
    closesocket(mSocket);
}

int TcpSocket::Connect(const SocketAddress& _addr)
{
    int err = connect(mSocket, &_addr.mSocketAddress, _addr.GetSize());

    if (err < 0)
    {
        SocketUtils::ReportError("TCPSocket::Connect");
        return -SocketUtils::GetLastError();
    }

    return NO_ERROR;
}

int TcpSocket::Bind(const SocketAddress& _addr)
{
    int error = bind(mSocket, &_addr.mSocketAddress, _addr.GetSize());

    if (error != 0)
    {
        SocketUtils::ReportError("TCPSocket::Bind");
        return SocketUtils::GetLastError();
    }

    return NO_ERROR;
}

int TcpSocket::Listen(int _backLog)
{
    int err = listen(mSocket, _backLog);

    if (err < 0)
    {
        SocketUtils::ReportError("TCPSocket::Listen");
        return -SocketUtils::GetLastError();
    }

    return NO_ERROR;
}

TcpSocketPtr TcpSocket::Accept(SocketAddress& _from)
{
    socklen_t length = _from.GetSize();
    SOCKET newSocket = accept(mSocket, &_from.mSocketAddress, &length);

    if (newSocket != INVALID_SOCKET)
    {
        return std::shared_ptr<TcpSocket>(new TcpSocket(newSocket));
    }
    else
    {
        SocketUtils::ReportError("TCPSocket::Accept");
        return nullptr;
    }
}

int32_t TcpSocket::Send(const void* _data, size_t _size)
{
    int bytesSentCount = send(mSocket,
        static_cast<const char*>(_data), static_cast<int>(_size), 0);

    if (bytesSentCount < 0)
    {
        SocketUtils::ReportError("TCPSocket::Send");
        return -SocketUtils::GetLastError();
    }

    return bytesSentCount;
}

int32_t TcpSocket::Receive(void* _buffer, size_t _size)
{
    int bytesReceivedCount = recv(mSocket,
        static_cast<char*>(_buffer), static_cast<int>(_size), 0);

    if (bytesReceivedCount < 0)
    {
        SocketUtils::ReportError("TCPSocket::Receive");
        return -SocketUtils::GetLastError();
    }

    return bytesReceivedCount;
}
