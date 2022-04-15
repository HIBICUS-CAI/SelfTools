#include "UdpSocket.h"
#include "SocketAddress.h"
#include "SocketUtils.h"
#include "LogUtils.h"

UdpSocket::UdpSocket(SOCKET _socket) :mSocket(_socket) {}

UdpSocket::~UdpSocket()
{
    closesocket(mSocket);
}

int UdpSocket::Bind(const class SocketAddress& _bind)
{
    int error = bind(mSocket, &_bind.mSocketAddress, _bind.GetSize());

    if (error)
    {
        SocketUtils::ReportError("UDPSocket::Bind");
        return SocketUtils::GetLastError();
    }

    return NO_ERROR;
}

int UdpSocket::SendTo(const void* _data, int _size, const SocketAddress& _to)
{
    int byteSentCount = sendto(mSocket, static_cast<const char*>(_data),
        _size, 0, &_to.mSocketAddress, _to.GetSize());

    if (byteSentCount <= 0)
    {
        SocketUtils::ReportError("UDPSocket::SendTo");
        return -SocketUtils::GetLastError();
    }
    else
    {
        return byteSentCount;
    }
}

int UdpSocket::ReceiveFrom(void* _data, int _maxSize, SocketAddress& _from)
{
    socklen_t fromLength = _from.GetSize();

    int readByteCount = recvfrom(mSocket,
        static_cast<char*>(_data),
        _maxSize,
        0, &_from.mSocketAddress, &fromLength);

    if (readByteCount >= 0)
    {
        return readByteCount;
    }
    else
    {
        int error = SocketUtils::GetLastError();

        if (error == WSAEWOULDBLOCK)
        {
            return 0;
        }
        else if (error == WSAECONNRESET)
        {
            PLOG("Connection reset from %s", _from.ToString().c_str());
            return -WSAECONNRESET;
        }
        else
        {
            SocketUtils::ReportError("UDPSocket::ReceiveFrom");
            return -error;
        }
    }
}

int UdpSocket::SetNonBlockingMode(bool _isNonBlocking)
{
    u_long arg = _isNonBlocking ? 1 : 0;
    int result = ioctlsocket(mSocket, FIONBIO, &arg);

    if (result == SOCKET_ERROR)
    {
        SocketUtils::ReportError("UDPSocket::SetNonBlockingMode");
        return SocketUtils::GetLastError();
    }
    else
    {
        return NO_ERROR;
    }
}
