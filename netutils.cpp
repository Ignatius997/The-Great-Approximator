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

namespace tga {
namespace net {

namespace {

/**
 * @brief Extracts the port number from a socket file descriptor for IPv4.
 * 
 * Function assumes, that sockfd is already bound and listening, thus
 * return value being 0 can be treated as an error.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in host byte order, 0 if an error occurs.
 * @note Auxiliary function to `extract_port()`.
 * @note This function assumes, that the socket is already bound and listening.
 */
uint16_t extract_port_ipv4(const int sockfd) {
    sockaddr_in addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        return ntohs(0); // Return 0 if error occurs.
    }
    return ntohs(addr.sin_port);
}

/**
 * @brief Extracts the port number from a socket file descriptor for IPv6.
 * 
 * Function assumes, that sockfd is already bound and listening, thus
 * return value being 0 can be treated as an error.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in host byte order, 0 if an error occurs.
 * @note Auxiliary function to `extract_port()`.
 * @note This function assumes, that the socket is already bound and listening.
 */
uint16_t extract_port_ipv6(const int sockfd) {
    sockaddr_in6 addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        return ntohs(0); // Return 0 if error occurs.
    }
    return ntohs(addr.sin6_port);
}

} // anonymous namespace

/**
 * @brief Binds a socket to a port and address.
 * @param sockfd The socket file descriptor.
 * @param port The port number in host byte order.
 * @param family The address family (AF_INET or AF_INET6).
 * @return 0 on success, 1 if an error occurs.
 */
int _bind(int &sockfd, uint16_t port, const int family) {
    assert(family == AF_INET || family == AF_INET6);

    sockaddr_in addr4{};
    sockaddr_in6 addr6{};
    struct sockaddr *cast_addr;
    socklen_t socklen = 0;

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
    } else return 1;

    if (bind(sockfd, cast_addr, socklen) < 0) {
        std::string fam = family == AF_INET ? "4" : "6";
        return 1; // Return 1 if error occurs.
    }

    return 0; // Successfully bound the socket.
}

/**
 * @brief Extracts the port number from a socket file descriptor.
 * 
 * Function assumes, that sockfd is already bound and listening, thus
 * return value being 0 can be treated as an error.
 * 
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 * @return The port number in host byte order.
 * @note Function assumes, that the socket is already bound and listening.
 */
uint16_t extract_port(const int sockfd, const int family) {
    if (family == AF_INET) {
        return extract_port_ipv4(sockfd);
    }

    return extract_port_ipv6(sockfd);
}

/**
 * @brief Gets the IP address from a SockAddrVariant.
 * @param addr The SockAddrVariant containing the address.
 * @return The IP address as a string.
 */
std::string get_ip(const SockAddrVariant &addr) {
    if (std::holds_alternative<sockaddr_in>(addr)) {
        char ip_str[INET_ADDRSTRLEN];
        const sockaddr_in *addr4 = std::get_if<sockaddr_in>(&addr);
        inet_ntop(AF_INET, &addr4->sin_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str);
    } else if (std::holds_alternative<sockaddr_in6>(addr)) {
        char ip_str[INET6_ADDRSTRLEN];
        const sockaddr_in6 *addr6 = std::get_if<sockaddr_in6>(&addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str);
    }

    assert(false); // Should never reach here
    return ""; // Just to satisfy the compiler, should never be reached.
}

/**
 * @brief Gets the port number from a SockAddrVariant.
 * @param addr The SockAddrVariant containing the address.
 * @return The port number in host byte order.
 */
uint16_t get_port(const SockAddrVariant &addr) {
    if (std::holds_alternative<sockaddr_in>(addr)) {
        const sockaddr_in *addr4 = std::get_if<sockaddr_in>(&addr);
        return (addr4->sin_port);
    } else if (std::holds_alternative<sockaddr_in6>(addr)) {
        const sockaddr_in6 *addr6 = std::get_if<sockaddr_in6>(&addr);
        return ntohs(addr6->sin6_port);
    }

    assert(false); // Should never reach here
    return 0; // Just to satisfy the compiler, should never be reached.
}

} // namespace net
} // namespace tga