#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <memory>
#include <poll.h>
#include <vector>
#include <netinet/in.h>

#include "netutils.h"
#include "global.h"
#include "args.h"
#include "log.h"

namespace tga {
namespace net {

// TODO Uncomment `#ifdef`
// #ifdef SERVER
namespace server {

namespace {

constexpr int socket_queue_len = 10; // FIXME Ulepszyć to
uint16_t port; // Stored in network order.
int ipv4_socket_fd = -1;
int ipv6_socket_fd = -1;
std::vector<pollfd> poll_descriptors;
size_t active_clients = 0;

constexpr size_t new_ipv4_clt_idx = 0;
constexpr size_t new_ipv6_clt_idx = 1;

/**
 * @brief Extracts the port number from a socket file descriptor for IPv4.
 * @param sockfd The socket file descriptor.
 * @return The port number in network byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv4(const int sockfd) {
    sockaddr_in addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::log::err::error("getsockname IPv4");
    }
    return addr.sin_port;
}

/**
 * @brief Extracts the port number from a socket file descriptor for IPv6.
 * @param sockfd The socket file descriptor.
 * @return The port number in network byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv6(const int sockfd) {
    sockaddr_in6 addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::log::err::error("getsockname IPv4");
    }
    return addr.sin6_port;
}

/**
 * @brief Extracts the port number from a socket file descriptor.
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 * @return The port number in network byte order.
 */
uint16_t extract_port(const int sockfd, const int family) {
    if (family == AF_INET) {
        return extract_port_ipv4(sockfd);
    }

    return extract_port_ipv6(sockfd);
}

/**
 * @brief Binds a socket to a port and address.
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 */
void _bind(int &sockfd, const int family) {
    sockaddr_in addr4{};
    sockaddr_in6 addr6{};
    struct sockaddr *cast_addr;
    socklen_t socklen;

    // Bind the socket to a concrete address.
    if (family == AF_INET) { // IPv4
        addr4.sin_family = family;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY); // Listening on all interfaces.
        addr4.sin_port = port;

        cast_addr = (struct sockaddr *) &addr4;
        socklen = (socklen_t) sizeof(addr4);
    } else if (family == AF_INET6) { // IPv6
        addr6.sin6_family = family;
        addr6.sin6_addr = in6addr_any;
        addr6.sin6_port = port;

        cast_addr = (struct sockaddr *) &addr6;
        socklen = (socklen_t) sizeof(addr6);
    }

    if (bind(sockfd, cast_addr, socklen) < 0) {
        std::string fam = family == AF_INET ? "4" : "6";
        tga::log::err::error("bind IPv" + fam);
    }
}

} // TODO needed? // anonymous namespace

// NOTE Code taken from laboratories.

/**
 * @brief Sets up a socket for listening.
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 * @note Code based on the code from laboratories.
 */
void setup_socket(int &sockfd, const int family) {
    // Create a socket.
    sockfd = socket(family, SOCK_STREAM, 0);
    if (sockfd < 0) {
        tga::log::err::error("cannot create a socket");
    }

    _bind(sockfd, family);

    // Switch the socket to listening.
    if (listen(sockfd, socket_queue_len) < 0) {
        tga::log::err::error("listen");
    }

    if (ntohs(port) == 0) {
        port = extract_port(sockfd, family);
    }
}

/**
 * @brief Sets up the server sockets for IPv4 and IPv6.
 * @note Code based on the code from laboratories.
 */
void setup() {
    port = htons(static_cast<uint16_t>(tga::args::port()));
    setup_socket(ipv4_socket_fd, AF_INET);
    setup_socket(ipv6_socket_fd, AF_INET6);

    poll_descriptors.reserve(2);
    poll_descriptors.at(new_ipv4_clt_idx) = (pollfd) {
        .fd = ipv4_socket_fd,
        .events = POLLIN,
    };
    poll_descriptors.at(new_ipv6_clt_idx) = (pollfd) {
        .fd = ipv6_socket_fd,
        .events = POLLIN,
    };
}

} // namespace server
// #endif // SERVER

#ifdef CLIENT
namespace client {

void setup() {

}

} // namespace client
#endif // CLIENT

/**
 * @brief Resolves a hostname to an IPv4 sockaddr_in structure for TCP connections.
 *
 * @param host Hostname or IPv4 address as a string.
 * @param port TCP port number (host byte order).
 * @return sockaddr_in structure ready to use for connect() or bind().
 *
 * @throws std::runtime_error if address resolution fails.
 */
struct sockaddr_in get_server_address(const std::string &host, unsigned port) {
    struct addrinfo hints;
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *address_result = nullptr;
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> address_guard(nullptr, freeaddrinfo);

    int errcode = getaddrinfo(host.c_str(), nullptr, &hints, &address_result);
    address_guard.reset(address_result);

    if (errcode != 0) {
        tga::log::err::error(std::string("getaddrinfo: ") + gai_strerror(errcode));
        exit(1);
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;
    send_address.sin_addr.s_addr =
        reinterpret_cast<sockaddr_in*>(address_result->ai_addr)->sin_addr.s_addr;
    send_address.sin_port = htons(static_cast<uint16_t>(port));

    return send_address;
}

/**
 * @brief Sets up the network for server or client.
 */
void setup() {
    #ifdef SERVER
    tga::net::server::setup();
    #endif

    #ifdef CLIENT
    tga::net::client::setup();
    #endif
}

} // namespace net
} // namespace tga