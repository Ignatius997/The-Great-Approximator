#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <memory>
#include <vector>
#include <netinet/in.h>
#include <variant>
#include <cassert>
#include <sys/socket.h>
#include <unistd.h>

#include "netutils.h"
#include "config.h"
#include "args.h"
#include "io.h"

namespace tga {
namespace net {

namespace {

/**
 * @brief Extracts the port number from a socket file descriptor for IPv4.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in host byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv4(const int sockfd) {
    sockaddr_in addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::io::log::err::error("getsockname IPv4");
    }
    return ntohs(addr.sin_port);
}

/**
 * @brief Extracts the port number from a socket file descriptor for IPv6.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in host byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv6(const int sockfd) {
    sockaddr_in6 addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::io::log::err::error("getsockname IPv4");
    }
    return ntohs(addr.sin6_port);
}

} // anonymous namespace

/**
 * @brief Binds a socket to a port and address.
 * @param sockfd The socket file descriptor.
 * @param port The port number in host byte order.
 * @param family The address family (AF_INET or AF_INET6).
 */
void _bind(int &sockfd, uint16_t port, const int family) {
    sockaddr_in addr4{};
    sockaddr_in6 addr6{};
    struct sockaddr *cast_addr;
    socklen_t socklen;

    // Bind the socket to a concrete address.
    if (family == AF_INET) { // IPv4
        addr4.sin_family = family;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
        addr4.sin_port = htons(port);

        cast_addr = (struct sockaddr *) &addr4;
        socklen = (socklen_t) sizeof(addr4);
    } else if (family == AF_INET6) { // IPv6
        addr6.sin6_family = family;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = htons(port);

        cast_addr = (struct sockaddr *) &addr6;
        socklen = (socklen_t) sizeof(addr6);
    }

    if (bind(sockfd, cast_addr, socklen) < 0) {
        std::string fam = family == AF_INET ? "4" : "6";
        tga::io::log::err::error("bind IPv" + fam);
    }
}

/**
 * @brief Extracts the port number from a socket file descriptor.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 * @return The port number in host byte order.
 */
uint16_t extract_port(const int sockfd, const int family) {
    if (family == AF_INET) {
        return extract_port_ipv4(sockfd);
    }

    return extract_port_ipv6(sockfd);
}

} // namespace net
} // namespace tga