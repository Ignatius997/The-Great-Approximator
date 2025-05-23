#ifndef TGA_NUTILS_H
#define TGA_NUTILS_H

#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <variant>
#include <cstring>

// ==== Comparison operators for tga::net::SockAddrVariant ====

inline bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_family == b.sin_family &&
           a.sin_port == b.sin_port &&
           a.sin_addr.s_addr == b.sin_addr.s_addr;
}
inline bool operator<(const sockaddr_in& a, const sockaddr_in& b) {
    if (a.sin_family != b.sin_family) return a.sin_family < b.sin_family;
    if (a.sin_port != b.sin_port) return a.sin_port < b.sin_port;
    return a.sin_addr.s_addr < b.sin_addr.s_addr;
}

inline bool operator==(const sockaddr_in6& a, const sockaddr_in6& b) {
    return a.sin6_family == b.sin6_family &&
           a.sin6_port == b.sin6_port &&
           std::memcmp(&a.sin6_addr, &b.sin6_addr, sizeof(in6_addr)) == 0;
}
inline bool operator<(const sockaddr_in6& a, const sockaddr_in6& b) {
    if (a.sin6_family != b.sin6_family) return a.sin6_family < b.sin6_family;
    if (a.sin6_port != b.sin6_port) return a.sin6_port < b.sin6_port;
    return std::memcmp(&a.sin6_addr, &b.sin6_addr, sizeof(in6_addr)) < 0;
}

namespace tga {
namespace net {

namespace client {
    int get_sockfd();
}

using SockAddrVariant = std::variant<sockaddr_in, sockaddr_in6>;

void setup();

} // namespace net
} // namespace tga

#endif // TGA_NUTIL_H