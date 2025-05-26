#include <unistd.h>
#include <string>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cassert>
#include <queue>
#include <map>
#include <utility>
#include <sys/time.h>
#include <memory>

#include "communication.h"
#include "utils.h"
#include "config.h"
#include "io.h"
#include "Message.h"
#include "MessageCombinators.h"
#include "netutils.h"
#include "args.h"
#include "clients-manager.h"

namespace tga {
namespace comm {

using tga::net::SockAddrVariant;
using tga::msg::Message;

namespace server {

namespace {

/** Time in seconds to wait for HELLO message
 * from the client from the moment of connection.
 */
constexpr time_t hello_to_sec = 3;

const std::string empty_string = ""; // Empty string for client ID.

constexpr size_t buffer_size = (1 << 13); // 8 KiB
std::vector<std::vector<char>> buffers; // Buffers for receiving messages from clients.

size_t messages_to_receive_ = 0; // Number of messages to receive from clients.
constexpr int socket_queue_len = 10; // FIXME Ulepszyć to?
uint16_t port; // Stored in host byte order.
int ipv4_socket_fd = -1;
int ipv6_socket_fd = -1;

/**
 * @brief Poll descriptors for the server.
 * @note If a socket is closed, its fd is set to -1.
 */
std::vector<pollfd> poll_descriptors;
std::queue<size_t> free_poll_indices; // Indexes of free poll descriptors.

class ClientConnection {
protected:
    std::string client_id = empty_string; // Client ID, set after HELLO message, empty in default.
    SockAddrVariant addr; // Address of the client.

    char buffer[buffer_size] = {0}; // Buffer for receiving and sending messages from the client.
    size_t buffer_len = 0; // Length of the buffer, i.e., how many valid bytes are currently in the buffer.
    size_t buffer_pos = 0; // Position in the buffer, i.e., how many bytes were already written to the buffer.

    bool baptised_ = false; // Whether the client has been baptised (i.e., has a name).

public:
    ClientConnection(const SockAddrVariant &addr) : addr(addr) {}

    /**
     * @brief Get the client ID.
     * @return The client ID as a string.
     */
    std::string get_client_id() const {
        assert(baptised_);
        return client_id;
    }

    /**
     * @brief Get the address of the client.
     * @return The address of the client as a SockAddrVariant.
     */
    SockAddrVariant get_addr() const {
        return addr;
    }

    /**
     * @brief Give name to the already connected, but unnamed client.
     */
    void baptise(const std::string &name) {
        assert(!baptised_);
        client_id = name;
        baptised_ = true;
    }

    /**
     * @brief Returns whether the client has been baptised.
     * @return true if the client has a name, false otherwise.
     */
    [[nodiscard]]
    bool baptised() const {
        return baptised_;
    }

    /**
     * @brief Returns pointer to the client's buffer.
     * @return pointer to the client's buffer.
     */
    char *get_buffer() {
        return buffer;
    }
};

/**
 * @brief Map of client indexes in `poll_descriptors` to their connection data.
 * The key is the index of the poll descriptor, and the value is a ClientConnection object.
 */
std::map<size_t, ClientConnection> connections;
// NOTE Number of active clients can be extracted by `connections.size()`.

auto cmp = [](const std::pair<size_t, timeval> &a, const std::pair<size_t, timeval> &b) {
    if (a.second.tv_sec != b.second.tv_sec) {
        return a.second.tv_sec > b.second.tv_sec;
    } else if (a.second.tv_usec != b.second.tv_usec) {
        return a.second.tv_usec > b.second.tv_usec;
    }
    return a.first > b.first;
};

std::priority_queue<
    std::pair<size_t, timeval>,
    std::vector<std::pair<size_t, timeval>>,
    decltype(cmp)
    > timeouts(cmp);

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

    if (tga::net::_bind(sockfd, htons(port), family) == 1) {
        tga::io::log::err::error("bind IPv" + family);
        exit(1);
    }

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
        if (port == 0) {
            tga::io::log::err::error("getsockname");
            exit(1);
        }
    }
}

void close_client_connection(const int idx) {
    if (tga::config::debug) tga::io::log::err::error("closing connection");
    
    auto it = connections.find(idx);
    if (it != connections.end()) {
        ClientConnection &conn = it->second;
        tga::cltman::deregister_client(conn.get_client_id(), conn.get_addr());
        connections.erase(it);
    }
    
    close(poll_descriptors.at(idx).fd);
    
    poll_descriptors.at(idx).fd = -1;
    free_poll_indices.push(idx);
}

/**
 * @brief Reads a message from the client.
 * 
 * This function reads a message from the client and returns the length of
 * the received message. If an error occurs, it closes the client connection.
 * 
 * @param idx The index of the client in the poll descriptors.
 * @return The length of the received message, or -1 on error.
 * 
 * @note This function not only calls `read` function, but also handles
 *       `read` error and the end of the connection (EOF).
 *       However, it leaves handling received message to the caller.
 */
ssize_t read_message(const size_t idx) {
    ssize_t len_received = read(poll_descriptors.at(idx).fd, connections.at(idx).get_buffer(), 0);

    if (len_received < 0) { // TODO Czy należy zamykać to połączenie
        tga::io::log::err::error("read from existing connection");
        close_client_connection(idx);
    } else if (len_received == 0) { // EOF
        // TODO Można napisać taki log.
        // tga::io::log::info::server::client_disconnected(
        //         tga::net::get_ip(idx_clt_map.at(i).second),
        //         tga::net::get_port(idx_clt_map.at(i).second));
        close_client_connection(idx);
    }

    return len_received;
}

/**
 * @brief Registers a new client connection.
 * 
 * This function registers a new client connection by adding it to the
 * poll descriptors and initializing the client connection data.
 * 
 * @param client_fd The file descriptor of the client socket.
 * @param client_addr The address of the client.
 * @return The index of the registered client in the poll descriptors.
 */
size_t register_new_client(const int client_fd, const SockAddrVariant &client_addr) {
    size_t idx;

    if (free_poll_indices.empty()) {
        pollfd new_client_desc = {
            .fd = client_fd,
            .events = POLLIN,
            .revents = 0,
        };
        poll_descriptors.push_back(new_client_desc);
        idx = poll_descriptors.size() - 1;
    } else {
        idx = free_poll_indices.front();
        free_poll_indices.pop();
        poll_descriptors.at(idx) = (pollfd) {
            .fd = client_fd,
            .events = POLLIN,
            .revents = 0,
        };
    }

    tga::io::log::info::server::new_client(
            tga::net::get_ip(client_addr),
            tga::net::get_port(client_addr));

    // Register only the address of the client.
    // The client shall be outrightly registered only after HELLO message.
    connections.at(idx) = ClientConnection(client_addr);
    return idx;
}

/**
 * @brief Accepts new clients on the given socket.
 * 
 * This function accepts new clients on the given socket and registers them
 * for further communication. It handles both IPv4 and IPv6 addresses.
 * 
 * @param family The address family (AF_INET or AF_INET6).
 * @param client_addr The address of the client.
 * @return The file descriptor of the accepted client.
 * @note This function is used internally by the server to accept new clients.
 * @note The function returns -1 if no more clients can be accepted.
 */
int accept_new_client(const int family, SockAddrVariant &client_addr) {
    assert(family == AF_INET || family == AF_INET6);

    int sockfd;
    sockaddr *addr;
    socklen_t addr_len{};
    
    if (family == AF_INET) {
        sockfd = ipv4_socket_fd;
        addr = (sockaddr *) (&std::get<sockaddr_in>(client_addr));
        addr_len = (socklen_t) sizeof(sockaddr_in);
    } else {
        sockfd = ipv6_socket_fd;
        addr = (sockaddr *) (&std::get<sockaddr_in6>(client_addr));
        addr_len = (socklen_t) sizeof(sockaddr_in6);
    }

    int client_fd = accept(sockfd, addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) { // No more clients to accept
            client_fd = -1;
        } else {
            tga::io::log::err::error("accept");
            exit(1);
        }
    }
    
    return client_fd;
}

using tga::msg::MsgPtr;

class MessageHandler {
public:
    virtual ~MessageHandler() = default;

    /**
     * @brief Handles the received message.
     * 
     * This function should be overridden by derived classes to handle
     * specific types of messages.
     * 
     * @param msg The received message.
     * @param rinfo Information about the received message.
     */
    virtual void handle(const MsgPtr &msg, ReceiveInfo &rinfo) = 0;
};

class HelloHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class CoeffHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class PutHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class BadPutHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class StateHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class PenaltyHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class ScoringHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

using MsgHandlerPtr = std::unique_ptr<MessageHandler>;

MsgHandlerPtr make_handler(const std::string &msg_type) {
        if (msg_type == "HELLO") {
            return std::make_unique<HelloHandler>();
        } else if (msg_type == "COEFF") {
            return std::make_unique<CoeffHandler>();
        } else if (msg_type == "STATE") {
            return std::make_unique<StateHandler>();
        } else if (msg_type == "SCORING") {
            return std::make_unique<ScoringHandler>();
        } else if (msg_type == "PUT") {
            return std::make_unique<PutHandler>();
        } else if (msg_type == "BAD_PUT") {
            return std::make_unique<BadPutHandler>();
        } else if (msg_type == "PENALTY") {
            return std::make_unique<PenaltyHandler>();
        } else {
            tga::io::log::err::error("unknown message type: " + msg_type);
            return nullptr;
        }
    }

void handle_received_message(const size_t idx, const size_t len_received, ReceiveInfo &rinfo) {
    assert(len_received > 0);

    const char *buffer = connections.at(idx).get_buffer();
    MsgPtr msg = tga::msg::deserialize_message(buffer, len_received, rinfo);
    
    if (msg == nullptr) { // Invalid message
        tga::io::log::err::error("invalid message received from client");
        return;
    } else if (rinfo.err != 0) { // Error during deserialization
        const ClientConnection &conn = connections.at(idx);
        std::string player_id = conn.baptised() ? conn.get_client_id() : empty_string;
        auto ip = connections.at(idx).get_addr();
        tga::io::log::err::message(rinfo.msg_type, player_id, ip);
        return;
    }
    
    // TODO Implement MessageHandler
    MsgHandlerPtr handler = make_handler(msg->messageType());
    handler->handle(msg, rinfo);
}

} // anonymous namespace

/**
 * @brief Returns number of correct messages left to receive from the clients
 */
size_t messages_to_receive() {
    return messages_to_receive_;
}

/**
 * @brief Return the size of the poll structure
 */
size_t poll_structure_size() {
    return poll_descriptors.size();
}

void update_timeouts() {
    // TODO Implement
}

bool connection_exists(const size_t idx) {
    assert(idx < poll_descriptors.size());
    return connections.find(idx) != connections.end();
    // TODO Co z tym fantem zrobić && poll_descriptors.at(idx).fd != -1;
}

/**
 * @brief Sets up the server socket and prepares it for accepting connections.
 * @note Code based on the code from laboratories.
 */
void setup() {
    messages_to_receive_ = tga::args::server::M();
    port = static_cast<uint16_t>(tga::args::port());
    setup_socket(ipv4_socket_fd, AF_INET);
    setup_socket(ipv6_socket_fd, AF_INET6);

    // Configure two first descriptors to await for new connections.
    poll_descriptors.reserve(2);
    poll_descriptors.push_back( (pollfd) {
        .fd = ipv4_socket_fd,
        .events = POLLIN,
        .revents = 0,
    });
    poll_descriptors.push_back( (pollfd) {
        .fd = ipv6_socket_fd,
        .events = POLLIN,
        .revents = 0,
    });
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
 * @brief Accepts new clients of given family.
 * 
 * This function accepts new clients on the given socket and registers them
 * for further communication. It handles both IPv4 and IPv6 addresses.
 * 
 * @param family The address family (AF_INET or AF_INET6).
 */
void new_clients(int family) {
    assert(family == AF_INET || family == AF_INET6);

    while (true) {
        SockAddrVariant client_addr{};
        
        int client_fd = accept_new_client(family, client_addr);
        if (client_fd < 0) break; // No more clients to accept
        
        // Set the client socket to non-blocking mode.
        if (fcntl(client_fd, F_SETFL, O_NONBLOCK)) {
            tga::io::log::err::error("fcntl");
            exit(1);
        }

        size_t idx = register_new_client(client_fd, client_addr);
        
        // Set timeout for new client;
        timeval hello_to { .tv_sec = hello_to_sec, .tv_usec = 0};
        timeouts.push(std::make_pair(idx, hello_to));
    }
}

bool new_ipv4_clients() {
    if (poll_descriptors.at(new_ipv4_clt_idx).revents & POLLIN) {
        return true;
    }
    return false;
}

bool new_ipv6_clients() {
    if (poll_descriptors.at(new_ipv6_clt_idx).revents & POLLIN) {
        return true;
    }
    return false;
}

/**
 * @brief Handles a poll event for a known client.
 * 
 * This function processes the poll event for a known client,
 * checking if there is data to read, write or if there is an error.
 * 
 * @param idx The index of the client in the poll descriptors.
 * @param rinfo The ReceiveInfo object to store information
 *              about possible received message.
 * 
 * @note This function is called when a poll event occurs for SOME client,
 *       but maybe not for THIS client.
 */
void handle_poll_event(const size_t idx, ReceiveInfo &rinfo) {
    if ((poll_descriptors.at(idx).revents & (POLLIN | POLLERR)) != 0) {
        ssize_t len_received = read_message(idx);
        if (len_received > 0) handle_received_message(idx, (size_t) len_received, rinfo);
    }

    if ((poll_descriptors.at(idx).revents & POLLOUT) != 0) {
        // TODO Implement
    }
}

/**
 * @brief Polls in-out events from the clients.
 */
int poll_events() {
    const auto &tv = timeouts.top().second;
    return poll(poll_descriptors.data(),
                (nfds_t) poll_descriptors.size(),
                tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * @brief Ends the communication.
 * 
 * This function sends SCORING messages to clients, closes connections,
 * and exits the program.
 */
void end() {
    // send SCORING messages to clients
    // close connections
    // exit
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

// ReceiveInfo receive_message() {
//     if (tga::config::server) return tga::comm::server::receive_message();
//     return tga::comm::client::receive_message();
// }

} // comm
} // tga