#include <unistd.h>
#include <string>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "communication.h"
#include "utils.h"
#include "config.h"
#include "io.h"
#include "Message.h"
#include "netutils.h"
#include "args.h"

namespace tga {
namespace comm {

using tga::msg::Message;
using tga::utils::ReceiveInfo;

namespace server {

namespace {

constexpr int socket_queue_len = 10; // FIXME Ulepszyć to
uint16_t port; // Stored in host byte order.
int ipv4_socket_fd = -1;
int ipv6_socket_fd = -1;
std::vector<pollfd> poll_descriptors;
size_t active_clients = 0;

/** Indexes of sockets awaiting for new clients. */
constexpr size_t new_ipv4_clt_idx = 0;
constexpr size_t new_ipv6_clt_idx = 1;

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
        exit(1);
    }

    tga::net::_bind(sockfd, port, family);

    // Set socket to nonblocking mode.
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK)) {
        tga::io::log::err::error("fcntl");
        exit(1);
    }

    // Switch the socket to listening.
    if (listen(sockfd, socket_queue_len) < 0) {
        tga::io::log::err::error("listen");
        exit(1);
    }

    if (port == 0) { // Port was not given arbitrarily in program arguments.
        port = tga::net::extract_port(sockfd, family);
    }
}

} // anonymous namespace

/**
 * @brief Sets up the server socket and prepares it for accepting connections.
 * @note Code based on the code from laboratories.
 */
void setup() {
    port = static_cast<uint16_t>(tga::args::port());
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

/**
 * @brief Clears the revents field of the poll descriptors.
 * 
 * This function is used to reset the revents field of the poll descriptors
 * to 0 after processing events.
 */
void clear_revents() {
    for (auto &poll_desc : poll_descriptors) {
        poll_desc.revents = 0;
    }
}

/**
 * @brief Polls the sockets for events.
 * 
 * This function waits for events on the sockets and returns the number of
 * events that occurred. It also handles the case when a socket is closed.
 * 
 * @return The number of events that occurred.
 */
int poll_events() {
    int poll_status = poll(poll_descriptors.data(), (nfds_t) poll_descriptors.size(), -1);
    if (poll_status < 0) {
        tga::io::log::err::error("poll");
        exit(1);
    }

    for (size_t i = 0; i < poll_descriptors.size(); ++i) {
        if (poll_descriptors.at(i).revents & POLLHUP) {
            close(poll_descriptors.at(i).fd);
            poll_descriptors.erase(poll_descriptors.begin() + i);
            --i;
        }
    }

    return poll_status;
}

ReceiveInfo receive_message() {
    tga::comm::server::clear_revents(); // NOTE This may be optimized.
    // TODO Maybe handle Ctrl-C like in echo-server-nonblocking.c
    int poll_status = tga::comm::server::poll_events();

    return ReceiveInfo{};
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
        exit(1);
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
        exit(1);
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
        exit(1);
    }
    
    tga::io::log::info::client::connected_to(tga::args::client::server(), port);
}

/**
 * @brief Returns the socket file descriptor.
 * 
 * @return The socket file descriptor.
 */
int get_sockfd() {
    return socket_fd;
}

/**
 * @brief Receives a message from the server.
 * 
 * This function reads a message from the server and returns it as a
 * ReceiveInfo object.
 * 
 * @return The received message as a ReceiveInfo object.
 */
ReceiveInfo receive_message() {
    // TODO Implement
    return ReceiveInfo{};
}
    
} // namespace client

/**
 * @brief Sets up the network for server or client.
 */
void setup() {
    if (tga::config::server) {
        tga::comm::server::setup();
    } else {
        tga::comm::client::setup();
    }
}

void send_message(int fd, const Message &msg) {
    std::string serialized_message = msg.serialize();

    // TODO Można to rozbić na namespace'y
    if (tga::config::server) {
        // TODO Implement
    } else {
        ssize_t len_sent = write(fd, serialized_message.c_str(), serialized_message.size());
        if (len_sent == -1) {
            tga::io::log::err::error("`write`");
            // TODO Handle error
        } else if (len_sent != static_cast<ssize_t>(serialized_message.size())) {
            tga::io::log::err::error("partial `write`: expected " +
                    std::to_string(serialized_message.size()) +
                    ", got " + std::to_string(len_sent));
            // TODO Handle partial write - Czy wysyłamy ponownie?
        }
    }
}

ReceiveInfo receive_message() {
    if (tga::config::server) return tga::comm::server::receive_message();
    return tga::comm::client::receive_message();
}

void handle_message(const ReceiveInfo &info) {
    (void) info;
    // TODO Implement this function
}

void end() {
    // send SCORING messages to clients
    // close connections
    // exit
}

} // comm
} // tga