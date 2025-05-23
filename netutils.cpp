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
#include <variant>
#include <cassert>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "netutils.h"
#include "config.h"
#include "args.h"
#include "io.h"

namespace tga {
namespace net {

namespace server {

namespace {

constexpr int socket_queue_len = 10; // FIXME Ulepszyć to
uint16_t port; // Stored in network order.
int ipv4_socket_fd = -1;
int ipv6_socket_fd = -1;
std::vector<pollfd> poll_descriptors;
size_t active_clients = 0;

/** Indexes of sockets awaiting for new clients. */
constexpr size_t new_ipv4_clt_idx = 0;
constexpr size_t new_ipv6_clt_idx = 1;

/**
 * @brief Extracts the port number from a socket file descriptor for IPv4.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in network byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv4(const int sockfd) {
    sockaddr_in addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::io::log::err::error("getsockname IPv4");
    }
    return addr.sin_port;
}

/**
 * @brief Extracts the port number from a socket file descriptor for IPv6.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
 * @param sockfd The socket file descriptor.
 * @return The port number in network byte order.
 * @note Auxiliary function to `extract_port()`.
 */
uint16_t extract_port_ipv6(const int sockfd) {
    sockaddr_in6 addr{};
    socklen_t length = (socklen_t) sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *) &addr, &length) < 0) {
        tga::io::log::err::error("getsockname IPv4");
    }
    return addr.sin6_port;
}

/**
 * @brief Extracts the port number from a socket file descriptor.
 * 
 * Function assumes, that sockfd is already bound and listening.
 * 
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

// TODO Better name?
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
        tga::io::log::err::error("bind IPv" + fam);
    }
}

} // TODO needed? // anonymous namespace

// NOTE Code taken from laboratories.

/**
 * @brief Sets up server socket for listening.
 * 
 * This function creates a socket, binds it to a port, and sets it to
 * non-blocking mode. It also switches the socket to listening mode.
 * 
 * @param sockfd The socket file descriptor.
 * @param family The address family (AF_INET or AF_INET6).
 * @note Code based on the code from laboratories.
 */
void setup_socket(int &sockfd, const int family) {
    // Create a socket.
    sockfd = socket(family, SOCK_STREAM, 0);
    if (sockfd < 0) {
        tga::io::log::err::error("cannot create a socket");
        exit(1); // TODO To exit or not to exit, that is the question.
    }

    _bind(sockfd, family);

    // Set socket to nonblocking mode.
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK)) {
        tga::io::log::err::error("fcntl");
        exit(1); // TODO To exit or not to exit, that is the question.
    }

    // Switch the socket to listening.
    if (listen(sockfd, socket_queue_len) < 0) {
        tga::io::log::err::error("listen");
        exit(1); // TODO To exit or not to exit, that is the question.
    }

    if (ntohs(port) == 0) { // Port was not given arbitrarily in program arguments.
        port = extract_port(sockfd, family);
    }
}

/**
 * @brief Sets up the server socket and prepares it for accepting connections.
 * @note Code based on the code from laboratories.
 */
void setup() {
    port = htons(static_cast<uint16_t>(tga::args::port()));
    setup_socket(ipv4_socket_fd, AF_INET);
    setup_socket(ipv6_socket_fd, AF_INET6);

    // Configure two first descriptors to await for new connections.
    poll_descriptors.reserve(2);
    poll_descriptors.at(new_ipv4_clt_idx) = (pollfd) {
        .fd = ipv4_socket_fd,
        .events = POLLIN,
        .revents = 0,
    };
    poll_descriptors.at(new_ipv6_clt_idx) = (pollfd) {
        .fd = ipv6_socket_fd,
        .events = POLLIN,
        .revents = 0,
    };
}

} // namespace server

namespace client {

namespace {

int socket_fd; // Socket file descriptor.
uint16_t port; // Stored in host byte order.
int family; // Address family (AF_INET or AF_INET6).

/**
 * @brief Resolves a hostname to a SockAddrVariant for TCP connections.
 *
 * @param host Hostname or IPv4/IPv6 address as a string.
 * @param port TCP port number (host byte order).
 * @return SockAddrVariant structure ready to use for connect() or bind().
 */
SockAddrVariant get_server_address(const std::string &host, const uint16_t port) {
    addrinfo hints{};
    // TODO Comment
    hints.ai_family = tga::args::client::family();
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *result = nullptr;
    int errcode = getaddrinfo(host.c_str(), nullptr, &hints, &result);
    if (errcode != 0 || result == nullptr) {
        tga::io::log::err::error(std::string("getaddrinfo: ") + gai_strerror(errcode));
        exit(1); // TODO To exit or not to exit, that is the question.
    }

    SockAddrVariant sock_addr;
    if (result->ai_family == AF_INET) { // IPv4
        sockaddr_in addr4{};
        std::memcpy(&addr4, result->ai_addr, sizeof(sockaddr_in));
        addr4.sin_port = htons(port);
        sock_addr = addr4;
        family = AF_INET;
    } else if (result->ai_family == AF_INET6) { // IPv6
        sockaddr_in6 addr6{};
        std::memcpy(&addr6, result->ai_addr, sizeof(sockaddr_in6));
        addr6.sin6_port = htons(port);
        sock_addr = addr6;
        family = AF_INET6;
    } else {
        tga::io::log::err::error("Unknown address family");
        freeaddrinfo(result);
        exit(1); // TODO To exit or not to exit, that is the question.
    }
    
    freeaddrinfo(result);
    return sock_addr;
}

} // anonymous namespace

/**
 * @brief Sets up the client socket and connects to the server.
 * @note Inspired by the code from laboratories.
 */
void setup() {
    port = static_cast<uint16_t>(tga::args::port());
    SockAddrVariant server_address = get_server_address(tga::args::client::server(), port);
    
    socket_fd = socket(family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        tga::io::log::err::error("cannot create a socket");
    }

    if (connect(socket_fd, (struct sockaddr *) &server_address,
                (socklen_t) sizeof(server_address)) < 0) {
        tga::io::log::err::error("connect");
        exit(1); // TODO To exit or not to exit, that is the question.
    }
    
    tga::io::log::info::client::connected_to(tga::args::client::server(), port);
}

int get_sockfd() {
    return socket_fd;
}

} // namespace client

/**
 * @brief Sets up the network for server or client.
 */
void setup() {
    if (tga::config::server) {
        tga::net::server::setup();
    } else {
        tga::net::client::setup();
    }
}

} // namespace net
} // namespace tga