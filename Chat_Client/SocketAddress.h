#pragma once

#include "BasicDef.h"

class SocketAddress
{
public:
    SocketAddress(uint32_t _address, uint16_t _port);

    SocketAddress(const sockaddr& _sockAddr);

    SocketAddress();

    bool operator==(const SocketAddress& _another) const;

    size_t GetHash() const;

    uint32_t GetSize() const;

    std::string ToString() const;

private:
    friend class TcpSocket;
    friend class UdpSocket;

    uint32_t& GetIP4Ref();

    const uint32_t& GetIP4Ref() const;

    sockaddr_in* GetAsSockAddrIn();

    const sockaddr_in* GetAsSockAddrIn() const;

    sockaddr mSocketAddress;
};

namespace std
{
    template<> struct hash<SocketAddress>
    {
        size_t operator()(const SocketAddress& _address) const
        {
            return _address.GetHash();
        }
    };
}

SocketAddressPtr CreateIPv4FromString(const std::string _addrInfo);
