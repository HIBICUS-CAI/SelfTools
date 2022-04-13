#pragma once

#include "BasicDef.h"

class SocketUtils
{
public:
    static int GetLastError();

    static void ReportError(std::string _desc);

    static bool SockInit();

    static void SocketStop();

    static int Select(const std::vector<TcpSocketPtr>* _inReadSet,
        std::vector<TcpSocketPtr>* _outReadSet,
        const std::vector<TcpSocketPtr>* _inWriteSet,
        std::vector<TcpSocketPtr>* _outWriteSet,
        const std::vector<TcpSocketPtr>* _inExceptSet,
        std::vector<TcpSocketPtr>* _outExceptSet);

    static TcpSocketPtr CreateTcpSocket(ADDRFAM _family);

    static UdpSocketPtr	CreateUdpSocket(ADDRFAM _family);

private:
    static fd_set* FillSetFromVector(fd_set& _outSet,
        const std::vector<TcpSocketPtr>* _inSockets);
    
    static void FillVectorFromSet(std::vector<TcpSocketPtr>* _outSockets,
        const std::vector<TcpSocketPtr>* _inSockets, const fd_set& _inSet);
};
