#include "SocketUtils.h"
#include "LogUtils.h"
#include "TcpSocket.h"
#include "UdpSocket.h"

int SocketUtils::GetLastError()
{
    return WSAGetLastError();
}

void SocketUtils::ReportError(std::string _desc)
{
    void* message = nullptr;
    DWORD errorNum = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&message, 0, NULL);

    std::wstring wMess = std::wstring((LPTSTR)message);
    std::string sMess = LogUtils::ConvertToString(wMess);

    PLOG("Error %s: %d - %s", _desc.c_str(), errorNum, sMess.c_str());
}

bool SocketUtils::SockInit()
{
    WSADATA wsaData = {};
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != NO_ERROR) { ReportError("Socket Starting Up"); return false; }
    else { return true; }
}

void SocketUtils::SocketStop()
{
    WSACleanup();
}

TcpSocketPtr SocketUtils::CreateTcpSocket(ADDRFAM _family)
{
    SOCKET s = socket((int)_family, SOCK_STREAM, IPPROTO_TCP);

    if (s != INVALID_SOCKET)
    {
        return TcpSocketPtr(new TcpSocket(s));
    }
    else
    {
        ReportError("SocketUtil::CreateTCPSocket");
        return nullptr;
    }
}

UdpSocketPtr SocketUtils::CreateUdpSocket(ADDRFAM _family)
{
    SOCKET s = socket((int)_family, SOCK_DGRAM, IPPROTO_UDP);

    if (s != INVALID_SOCKET)
    {
        return UdpSocketPtr(new UdpSocket(s));
    }
    else
    {
        ReportError("SocketUtil::CreateUDPSocket");
        return nullptr;
    }
}

fd_set* SocketUtils::FillSetFromVector(fd_set& _outSet,
    const std::vector<TcpSocketPtr>* _inSockets)
{
    if (_inSockets)
    {
        FD_ZERO(&_outSet);

        for (const TcpSocketPtr& socket : *_inSockets)
        {
            FD_SET(socket->mSocket, &_outSet);
        }

        return &_outSet;
    }
    else
    {
        return nullptr;
    }
}

void SocketUtils::FillVectorFromSet(std::vector<TcpSocketPtr>* _outSockets,
    const std::vector<TcpSocketPtr>* _inSockets, const fd_set& _inSet)
{
    if (_inSockets && _outSockets)
    {
        _outSockets->clear();
        for (const TcpSocketPtr& socket : *_inSockets)
        {
            if (FD_ISSET(socket->mSocket, &_inSet))
            {
                _outSockets->push_back(socket);
            }
        }
    }
}

int SocketUtils::Select(const std::vector<TcpSocketPtr>* _inReadSet,
    std::vector<TcpSocketPtr>* _outReadSet,
    const std::vector<TcpSocketPtr>* _inWriteSet,
    std::vector<TcpSocketPtr>* _outWriteSet,
    const std::vector<TcpSocketPtr>* _inExceptSet,
    std::vector<TcpSocketPtr>* _outExceptSet)
{
    fd_set read, write, except;

    fd_set* readPtr = FillSetFromVector(read, _inReadSet);
    fd_set* writePtr = FillSetFromVector(write, _inWriteSet);
    fd_set* exceptPtr = FillSetFromVector(except, _inExceptSet);

    int toRet = select(0, readPtr, writePtr, exceptPtr, nullptr);

    if (toRet > 0)
    {
        FillVectorFromSet(_outReadSet, _inReadSet, read);
        FillVectorFromSet(_outWriteSet, _inWriteSet, write);
        FillVectorFromSet(_outExceptSet, _inExceptSet, except);
    }

    return toRet;
}
