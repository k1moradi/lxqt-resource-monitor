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

#include "resourcemonitorpacket.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

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
                if (ResourceMonitorPacket::classifyPacket(
                        packetBuffer.data(),
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
