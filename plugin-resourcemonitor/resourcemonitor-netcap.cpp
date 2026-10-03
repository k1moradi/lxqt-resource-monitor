/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include <arpa/inet.h>
#include <linux/capability.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
constexpr auto ReportInterval = std::chrono::milliseconds(250);
constexpr std::size_t PacketBufferSize = 65536;

struct TrafficCounters
{
    std::uint64_t localReceive{0};
    std::uint64_t localTransmit{0};
    std::uint64_t internetReceive{0};
    std::uint64_t internetTransmit{0};
};

bool isLocalIpv4(std::uint32_t address)
{
    return (address & 0xff000000U) == 0x00000000U        // 0.0.0.0/8
        || (address & 0xff000000U) == 0x0a000000U        // 10.0.0.0/8
        || (address & 0xffc00000U) == 0x64400000U        // 100.64.0.0/10
        || (address & 0xff000000U) == 0x7f000000U        // 127.0.0.0/8
        || (address & 0xffff0000U) == 0xa9fe0000U        // 169.254.0.0/16
        || (address & 0xfff00000U) == 0xac100000U        // 172.16.0.0/12
        || (address & 0xffffff00U) == 0xc0000000U        // 192.0.0.0/24
        || (address & 0xffffff00U) == 0xc0000200U        // 192.0.2.0/24
        || (address & 0xffff0000U) == 0xc0a80000U        // 192.168.0.0/16
        || (address & 0xfffe0000U) == 0xc6120000U        // 198.18.0.0/15
        || (address & 0xffffff00U) == 0xc6336400U        // 198.51.100.0/24
        || (address & 0xffffff00U) == 0xcb007100U        // 203.0.113.0/24
        || (address & 0xf0000000U) == 0xe0000000U        // 224.0.0.0/4
        || (address & 0xf0000000U) == 0xf0000000U;       // 240.0.0.0/4
}

bool isLocalIpv6(const in6_addr &address)
{
    if (IN6_IS_ADDR_UNSPECIFIED(&address)
        || IN6_IS_ADDR_LOOPBACK(&address)
        || IN6_IS_ADDR_LINKLOCAL(&address)
        || IN6_IS_ADDR_SITELOCAL(&address)
        || IN6_IS_ADDR_MULTICAST(&address)
        || (address.s6_addr[0] & 0xfeU) == 0xfcU)          // fc00::/7
    {
        return true;
    }

    if (IN6_IS_ADDR_V4MAPPED(&address))
    {
        std::uint32_t ipv4Address = 0;
        std::memcpy(&ipv4Address, &address.s6_addr[12], sizeof(ipv4Address));
        return isLocalIpv4(ntohl(ipv4Address));
    }

    // IPv6 documentation prefix; it is not globally routable.
    return address.s6_addr[0] == 0x20U
        && address.s6_addr[1] == 0x01U
        && address.s6_addr[2] == 0x0dU
        && address.s6_addr[3] == 0xb8U;
}

bool dropCapabilities()
{
    __user_cap_header_struct header{};
    header.version = _LINUX_CAPABILITY_VERSION_3;
    header.pid = 0;
    std::array<__user_cap_data_struct, 2> data{};
    if (syscall(SYS_capset, &header, data.data()) != 0)
        return false;

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        return false;

    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    return true;
}

bool classifyPacket(const std::uint8_t *packet,
                    std::size_t packetLength,
                    std::uint16_t protocol,
                    bool outgoing,
                    bool &isLocal)
{
    if (protocol == ETH_P_IP && packetLength >= 20 && (packet[0] >> 4) == 4)
    {
        const std::size_t headerLength = static_cast<std::size_t>(packet[0] & 0x0fU) * 4;
        if (headerLength < 20 || packetLength < headerLength)
            return false;

        std::uint32_t sourceAddress = 0;
        std::uint32_t destinationAddress = 0;
        std::memcpy(&sourceAddress, packet + 12, sizeof(sourceAddress));
        std::memcpy(&destinationAddress, packet + 16, sizeof(destinationAddress));
        const std::uint32_t remoteAddress = ntohl(outgoing ? destinationAddress : sourceAddress);
        isLocal = isLocalIpv4(remoteAddress);
        return true;
    }

    if (protocol == ETH_P_IPV6 && packetLength >= 40 && (packet[0] >> 4) == 6)
    {
        in6_addr remoteAddress{};
        std::memcpy(&remoteAddress, packet + (outgoing ? 24 : 8), sizeof(remoteAddress));
        isLocal = isLocalIpv6(remoteAddress);
        return true;
    }

    return false;
}

} // namespace

int main()
{
    const int packetSocket = socket(AF_PACKET,
                                    SOCK_DGRAM | SOCK_CLOEXEC,
                                    htons(ETH_P_ALL));
    if (packetSocket < 0)
    {
        std::fprintf(stderr, "resourcemonitor-netcap: CAP_NET_RAW is required\n");
        return 1;
    }

    if (!dropCapabilities())
    {
        std::fprintf(stderr, "resourcemonitor-netcap: failed to drop capabilities\n");
        close(packetSocket);
        return 1;
    }

    const unsigned int loopbackIndex = if_nametoindex("lo");
    std::array<std::uint8_t, PacketBufferSize> packetBuffer{};
    TrafficCounters counters;
    auto nextReport = std::chrono::steady_clock::now() + ReportInterval;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(nextReport - now);
        const int timeout = static_cast<int>(remaining.count() > 0 ? remaining.count() : 0);
        pollfd descriptor{packetSocket, POLLIN, 0};
        const int pollResult = poll(&descriptor, 1, timeout);

        if (pollResult > 0 && (descriptor.revents & POLLIN) != 0)
        {
            sockaddr_ll source{};
            socklen_t sourceLength = sizeof(source);
            const ssize_t packetLength = recvfrom(packetSocket,
                                                  packetBuffer.data(),
                                                  packetBuffer.size(),
                                                  0,
                                                  reinterpret_cast<sockaddr *>(&source),
                                                  &sourceLength);
            if (packetLength > 0
                && source.sll_ifindex != static_cast<int>(loopbackIndex)
                && source.sll_pkttype != PACKET_LOOPBACK)
            {
                bool localDestination = false;
                if (classifyPacket(packetBuffer.data(),
                                   static_cast<std::size_t>(packetLength),
                                   ntohs(source.sll_protocol),
                                   source.sll_pkttype == PACKET_OUTGOING,
                                   localDestination))
                {
                    const std::uint64_t bytes = static_cast<std::uint64_t>(packetLength);
                    const bool outgoing = source.sll_pkttype == PACKET_OUTGOING;
                    if (localDestination)
                    {
                        if (outgoing)
                            counters.localTransmit += bytes;
                        else
                            counters.localReceive += bytes;
                    }
                    else if (outgoing)
                    {
                        counters.internetTransmit += bytes;
                    }
                    else
                    {
                        counters.internetReceive += bytes;
                    }
                }
            }
        }

        if (std::chrono::steady_clock::now() >= nextReport)
        {
            std::printf("%llu %llu %llu %llu\n",
                        static_cast<unsigned long long>(counters.localReceive),
                        static_cast<unsigned long long>(counters.localTransmit),
                        static_cast<unsigned long long>(counters.internetReceive),
                        static_cast<unsigned long long>(counters.internetTransmit));
            std::fflush(stdout);
            counters = {};
            nextReport = std::chrono::steady_clock::now() + ReportInterval;
        }
    }
}
