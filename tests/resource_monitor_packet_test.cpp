#include "resourcemonitorpacket.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void check(bool passed, const char *description)
{
    if (passed)
        return;

    std::cerr << "FAIL: " << description << '\n';
    ++failures;
}

bool ipv4IsLocal(const char *address)
{
    in_addr parsed{};
    if (inet_pton(AF_INET, address, &parsed) != 1)
        return false;
    return ResourceMonitorPacket::isLocalIpv4(ntohl(parsed.s_addr));
}

bool ipv6IsLocal(const char *address)
{
    in6_addr parsed{};
    if (inet_pton(AF_INET6, address, &parsed) != 1)
        return false;
    return ResourceMonitorPacket::isLocalIpv6(parsed);
}

std::array<std::uint8_t, 24> makeIpv4Packet(const char *source,
                                            const char *destination,
                                            std::uint8_t headerLengthWords = 5)
{
    std::array<std::uint8_t, 24> packet{};
    packet[0] = static_cast<std::uint8_t>(0x40U | (headerLengthWords & 0x0fU));
    (void)inet_pton(AF_INET, source, packet.data() + 12);
    (void)inet_pton(AF_INET, destination, packet.data() + 16);
    return packet;
}

std::array<std::uint8_t, 40> makeIpv6Packet(const char *source, const char *destination)
{
    std::array<std::uint8_t, 40> packet{};
    packet[0] = 0x60U;
    (void)inet_pton(AF_INET6, source, packet.data() + 8);
    (void)inet_pton(AF_INET6, destination, packet.data() + 24);
    return packet;
}
} // namespace

int main()
{
    check(ipv4IsLocal("0.0.0.1"), "IPv4 current-network range is local");
    check(ipv4IsLocal("10.255.255.255"), "10/8 is local through its upper boundary");
    check(!ipv4IsLocal("11.0.0.0"), "address after 10/8 is public");
    check(ipv4IsLocal("100.64.0.0"), "CGNAT range includes lower boundary");
    check(ipv4IsLocal("100.127.255.255"), "CGNAT range includes upper boundary");
    check(!ipv4IsLocal("100.128.0.0"), "address after CGNAT range is public");
    check(ipv4IsLocal("127.255.255.255"), "127/8 is local");
    check(ipv4IsLocal("169.254.0.1"), "IPv4 link-local is local");
    check(ipv4IsLocal("172.16.0.0"), "172.16/12 includes lower boundary");
    check(ipv4IsLocal("172.31.255.255"), "172.16/12 includes upper boundary");
    check(!ipv4IsLocal("172.32.0.0"), "address after 172.16/12 is public");
    check(ipv4IsLocal("192.168.0.0"), "192.168/16 is local");
    check(ipv4IsLocal("192.0.0.1"), "special-use 192.0.0/24 is local");
    check(ipv4IsLocal("192.0.2.1"), "IPv4 documentation range is local");
    check(ipv4IsLocal("198.18.0.0"), "benchmarking range includes lower boundary");
    check(ipv4IsLocal("198.19.255.255"), "benchmarking range includes upper boundary");
    check(ipv4IsLocal("198.51.100.1"), "IPv4 documentation range is local");
    check(ipv4IsLocal("203.0.113.1"), "IPv4 documentation range is local");
    check(ipv4IsLocal("224.0.0.1"), "IPv4 multicast is local traffic");
    check(ipv4IsLocal("240.0.0.1"), "reserved IPv4 range is local traffic");
    check(!ipv4IsLocal("8.8.8.8"), "ordinary public IPv4 address is public");
    check(!ipv4IsLocal("100.63.255.255"), "address immediately before CGNAT is public");

    check(ipv6IsLocal("::"), "IPv6 unspecified is local");
    check(ipv6IsLocal("::1"), "IPv6 loopback is local");
    check(ipv6IsLocal("fe80::1"), "IPv6 link-local is local");
    check(ipv6IsLocal("fec0::1"), "IPv6 site-local is local");
    check(ipv6IsLocal("fc00::1"), "IPv6 ULA lower range is local");
    check(ipv6IsLocal("fdff::1"), "IPv6 ULA upper range is local");
    check(ipv6IsLocal("ff02::1"), "IPv6 multicast is local traffic");
    check(ipv6IsLocal("2001:db8::1"), "IPv6 documentation range is local");
    check(!ipv6IsLocal("2606:4700:4700::1111"), "ordinary public IPv6 address is public");
    check(ipv6IsLocal("::ffff:192.168.1.1"), "IPv4-mapped private IPv6 address is local");
    check(!ipv6IsLocal("::ffff:8.8.8.8"), "IPv4-mapped public IPv6 address is public");

    bool isLocal = false;
    auto ipv4 = makeIpv4Packet("10.0.0.2", "8.8.8.8");
    check(ResourceMonitorPacket::classifyPacket(ipv4.data(), 20, ETH_P_IP, true, isLocal),
          "valid outgoing IPv4 header is accepted");
    check(!isLocal, "outgoing IPv4 uses destination as remote address");
    ipv4 = makeIpv4Packet("8.8.8.8", "10.0.0.2");
    check(ResourceMonitorPacket::classifyPacket(ipv4.data(), 20, ETH_P_IP, false, isLocal),
          "valid incoming IPv4 header is accepted");
    check(!isLocal, "incoming IPv4 uses source as remote address");
    ipv4 = makeIpv4Packet("10.0.0.2", "172.20.1.4");
    check(ResourceMonitorPacket::classifyPacket(ipv4.data(), 20, ETH_P_IP, true, isLocal)
              && isLocal,
          "outgoing private IPv4 destination is classified as local");
    ipv4 = makeIpv4Packet("192.168.1.20", "10.0.0.2");
    check(ResourceMonitorPacket::classifyPacket(ipv4.data(), 20, ETH_P_IP, false, isLocal)
              && isLocal,
          "incoming private IPv4 source is classified as local");

    auto ipv6 = makeIpv6Packet("fd00::2", "2606:4700:4700::1111");
    check(ResourceMonitorPacket::classifyPacket(ipv6.data(), ipv6.size(), ETH_P_IPV6, true, isLocal),
          "valid outgoing IPv6 header is accepted");
    check(!isLocal, "outgoing IPv6 uses destination as remote address");
    ipv6 = makeIpv6Packet("2001:db8::1", "fd00::2");
    check(ResourceMonitorPacket::classifyPacket(ipv6.data(), ipv6.size(), ETH_P_IPV6, false, isLocal)
              && isLocal,
          "incoming IPv6 uses source as remote address");

    check(!ResourceMonitorPacket::classifyPacket(nullptr, 20, ETH_P_IP, true, isLocal),
          "null packet is rejected");
    check(!ResourceMonitorPacket::classifyPacket(ipv4.data(), 19, ETH_P_IP, true, isLocal),
          "truncated IPv4 base header is rejected");
    auto malformedIpv4 = makeIpv4Packet("10.0.0.1", "8.8.8.8", 4);
    check(!ResourceMonitorPacket::classifyPacket(malformedIpv4.data(), 20, ETH_P_IP, true, isLocal),
          "IPv4 header shorter than the minimum is rejected");
    malformedIpv4 = makeIpv4Packet("10.0.0.1", "8.8.8.8", 6);
    check(!ResourceMonitorPacket::classifyPacket(malformedIpv4.data(), 20, ETH_P_IP, true, isLocal),
          "truncated IPv4 options are rejected");
    check(ResourceMonitorPacket::classifyPacket(malformedIpv4.data(), 24, ETH_P_IP, true, isLocal),
          "IPv4 options are accepted when fully present");
    check(!ResourceMonitorPacket::classifyPacket(ipv6.data(), ipv6.size(), ETH_P_IP, true, isLocal),
          "protocol and IP version mismatch is rejected");
    check(!ResourceMonitorPacket::classifyPacket(ipv6.data(), 39, ETH_P_IPV6, true, isLocal),
          "truncated IPv6 header is rejected");
    check(!ResourceMonitorPacket::classifyPacket(ipv4.data(), 20, ETH_P_ARP, true, isLocal),
          "unsupported packet protocol is rejected");

    if (failures != 0)
        std::cerr << failures << " resource monitor packet check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
