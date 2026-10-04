/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef LXQTRESOURCEMONITORPACKET_H
#define LXQTRESOURCEMONITORPACKET_H

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <netinet/in.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ResourceMonitorPacket
{
[[nodiscard]] inline bool isLocalIpv4(std::uint32_t address) noexcept
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

[[nodiscard]] inline bool isLocalIpv6(const in6_addr &address) noexcept
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

[[nodiscard]] inline bool classifyPacket(const std::uint8_t *packet,
                                         std::size_t packetLength,
                                         std::uint16_t protocol,
                                         bool outgoing,
                                         bool &isLocal) noexcept
{
    if (packet == nullptr)
        return false;

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
} // namespace ResourceMonitorPacket

#endif // LXQTRESOURCEMONITORPACKET_H
