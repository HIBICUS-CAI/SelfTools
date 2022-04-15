#include "SocketAddress.h"
#include "LogUtils.h"
#include "SocketUtils.h"

SocketAddress::SocketAddress(uint32_t _address, uint16_t _port)
{
    GetAsSockAddrIn()->sin_family = AF_INET;
    GetIP4Ref() = htonl(_address);
    GetAsSockAddrIn()->sin_port = htons(_port);
}

SocketAddress::SocketAddress(const sockaddr& _sockAddr)
{
    memcpy(&mSocketAddress, &_sockAddr, sizeof(sockaddr));
}

SocketAddress::SocketAddress()
{
    GetAsSockAddrIn()->sin_family = AF_INET;
    GetIP4Ref() = INADDR_ANY;
    GetAsSockAddrIn()->sin_port = 0;
}

bool SocketAddress::operator==(const SocketAddress& _another) const
{
    return
        (mSocketAddress.sa_family == _another.mSocketAddress.sa_family &&
            GetAsSockAddrIn()->sin_port == _another.GetAsSockAddrIn()->sin_port) &&
        (GetIP4Ref() == _another.GetIP4Ref());
}

size_t SocketAddress::GetHash() const
{
    return static_cast<size_t>((GetIP4Ref()) |
        ((static_cast<uint32_t>(GetAsSockAddrIn()->sin_port)) << 13) |
        mSocketAddress.sa_family);
}

uint32_t SocketAddress::GetSize() const
{
    return sizeof(sockaddr);
}

std::string SocketAddress::ToString() const
{
    const sockaddr_in* s = GetAsSockAddrIn();
    char temp[128];
    InetNtopA(s->sin_family, const_cast<in_addr*>(&s->sin_addr),
        temp, sizeof(temp));

    return LogUtils::Sprintf("%s:%d", temp, ntohs(s->sin_port));
}

uint32_t& SocketAddress::GetIP4Ref()
{
    return *reinterpret_cast<uint32_t*>(
        &(GetAsSockAddrIn()->sin_addr.S_un.S_addr));
}

const uint32_t& SocketAddress::GetIP4Ref() const
{
    return *reinterpret_cast<const uint32_t*>(
        &(GetAsSockAddrIn()->sin_addr.S_un.S_addr));
}

sockaddr_in* SocketAddress::GetAsSockAddrIn()
{
    return reinterpret_cast<sockaddr_in*>(&mSocketAddress);
}

const sockaddr_in* SocketAddress::GetAsSockAddrIn() const
{
    return reinterpret_cast<const sockaddr_in*>(&mSocketAddress);
}

SocketAddressPtr CreateIPv4FromString(const std::string _addrInfo)
{
    auto pos = _addrInfo.find_last_of(':');
    std::string host, service;
    if (pos != std::string::npos)
    {
        host = _addrInfo.substr(0, pos);
        service = _addrInfo.substr(pos + 1);
    }
    else
    {
        host = _addrInfo;
        service = "0";
    }
    addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;

    addrinfo* result;
    int error = getaddrinfo(host.c_str(), service.c_str(), &hint, &result);
    if (error != 0)
    {
        SocketUtils::ReportError("SocketAddressFactory::CreateIPv4FromString");
        if (result) { freeaddrinfo(result); }
        return nullptr;
    }

    while (!result->ai_addr && result->ai_next) { result = result->ai_next; }

    if (!result->ai_addr) { return nullptr; }

    auto toRet = std::make_shared<SocketAddress>(*result->ai_addr);

    freeaddrinfo(result);

    return toRet;
}
