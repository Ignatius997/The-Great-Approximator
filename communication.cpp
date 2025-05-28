#include <unistd.h>
#include <poll.h>
#include <sys/time.h>

#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <memory>
#include <string>
#include <cstring>
#include <cctype>
#include <cassert>

#include <algorithm>
#include <map>
#include <vector>
#include <queue>
#include <utility>

#include "communication.h"
#include "config.h"
#include "io.h"
#include "Message.h"
#include "MessageCombinators.h"
#include "netutils.h"
#include "args.h"
#include "Rational.h"

namespace tga {
namespace comm {

using tga::net::SockAddrVariant;
using tga::net::ReceivedDataStatus;
using tga::net::CommunicationPhase;

using tga::msg::Message;
using tga::msg::MsgPtr;

using tga::msg::HelloMessage;
using tga::msg::CoeffMessage;
using tga::msg::StateMessage;
using tga::msg::PutMessage;
using tga::msg::BadPutMessage;
using tga::msg::PenaltyMessage;
using tga::msg::ScoringMessage;

using tga::rat::Rational;

namespace server {

namespace {

/** Time to wait for HELLO message
 * from the client from the moment of connection.
 */
constexpr timeval hello_timeout { .tv_sec = 3, .tv_usec = 0};

/** Delay for sending BAD_PUT. */
constexpr timeval bad_put_delay { .tv_sec = 1, .tv_usec = 0 };

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

/**
 * @brief Determines meaning of the timeout that occurred for the client,
 *        that operates this enum.
 */
enum TimeoutMeaning {
    NO_MESSAGE_RECEIVED, // No message received from the was client in expected time.
    SEND_DELAY // Delay time in sending a message passed.
};

class ClientConnection {
protected:
    std::string client_id = empty_string; // Client ID, set after HELLO message, empty in default.
    SockAddrVariant addr; // Address of the client.

    char buffer[buffer_size] = {0}; // Buffer for receiving and sending messages from the client.
    size_t buffer_len = 0; // Length of the buffer, i.e., how many valid bytes are currently in the buffer.
    size_t buffer_pos = 0; // Position in the buffer, i.e., how many bytes were already written to the buffer.

    bool baptised_ = false; // Whether the client has been baptised (i.e., has a name).

    std::optional<TimeoutMeaning> timeout_meaning = std::nullopt; // Meaning of the timeout, if it occurred.
    std::optional<MsgPtr> timeout_message = std::nullopt; // Message to send after the timeout, if it occurred.

    // NOTE na razie unused.
    CommunicationPhase phase = CommunicationPhase::START;
    // FIXME Po pierwsze magiczna stała
    // FIXME Po drugie, to czy nie można tego zrobić w wektorze?
    Rational approximations[10001] = {Rational("0")}; // Approximations for the polynomial, indexed by point (0 to 10000).


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

    /**
     * @brief Adds a value to the approximation for the given point.
     * @param point The point index (0 to 10000).
     * @param value The value to add to the approximation (-5.0 to 5.0).
     * @note The function assumes, that point <- 10^4 and |value| <= 5.0.
     */
    void approxAdd(size_t point, double value) {
        assert(point <= 10000);
        assert(value >= -5.0 && value <= 5.0);
        approximations[point] += Rational(value);
    }

    /**
     * @brief Sets the meaning of the timeout that occurred for the client.
     * @param meaning The meaning of the timeout.
     */
    void setTimeoutMeaning(const TimeoutMeaning meaning) {
        timeout_meaning = meaning;
    }

    /**
     * @brief Returns the meaning of the timeout that occurred for the client.
     * @return The meaning of the timeout, or std::nullopt if no timeout occurred.
     */
    [[nodiscard]]
    std::optional<TimeoutMeaning> getTimeoutMeaning() const {
        return timeout_meaning;
    }

    /**
     * @brief Sets the message to send after the timeout.
     * @param msg The message to send after the timeout.
     * @note `msg` is moved into the object, so it should not be used after this call.
     */
    void setTimeoutMessage(MsgPtr msg) {
        timeout_message = std::move(msg);
    }

    /**
     * @brief Returns the message to send after the timeout.
     * @return The message to send after the timeout, or std::nullopt if no message is set.
     * @note The message is moved from the object, so it should not be used after this call.
     */
    [[nodiscard]]
    std::optional<MsgPtr> getTimeoutMessage() {
        auto msg = std::move(timeout_message);
        timeout_message = std::nullopt; // Clear the message after moving it.
        return msg;
    }

    std::vector<Rational> getApproximations() const {
        auto end = approximations + tga::args::server::K();
        return std::vector<Rational>(approximations, end);
    }

    timeval getStateDelay() const {
        time_t lowercase_count = std::count_if(
            client_id.begin(), client_id.end(),
            [](unsigned char c) { return std::islower(c); }
        );
        return { .tv_sec = lowercase_count, .tv_usec = 0 };
    }
};

/**
 * @brief Map of client indexes in `poll_descriptors` to their connection data.
 * The key is the index of the poll descriptor, and the value is a ClientConnection object.
 */
std::map<size_t, ClientConnection> connections;
// NOTE Number of active clients can be extracted by `connections.size()`.

/**
 * @brief Checks if timeout occured.
 * @param tv The timeval to check.
 * @return true if the timeval is nonpositive, false otherwise.
 */
bool is_timeout_expired(const timeval &tv) {
    return tv.tv_sec < 0 || (tv.tv_sec == 0 && tv.tv_usec <= 0);
}

/**
 * @brief The comparison function for ModifiableIdxTvPriorityQueue.
 */
auto cmp = [](const std::pair<size_t, timeval> &a, const std::pair<size_t, timeval> &b) {
    if (a.second.tv_sec != b.second.tv_sec) {
        return a.second.tv_sec > b.second.tv_sec;
    } else if (a.second.tv_usec != b.second.tv_usec) {
        return a.second.tv_usec > b.second.tv_usec;
    }
    return a.first > b.first;
};

/**
 * @brief A modifiable priority queue for managing timeouts.
 * 
 * This queue stores pairs of `<size_t index, timeval tv>` where
 * `index` is the index of the client in `poll_descriptors` and
 * `tv` is time left to timeout. 
 */
class ModifiableIdxTvPriorityQueue {
private:
    using Pair = std::pair<size_t, timeval>;
    std::vector<Pair> data;

public:
    void push(const Pair& p) {
        data.push_back(p);
        std::push_heap(data.begin(), data.end(), cmp);
    }

    const Pair& top() const {
        return data.front();
    }

    void pop() {
        std::pop_heap(data.begin(), data.end(), cmp);
        data.pop_back();
    }

    bool empty() const {
        return data.empty();
    }

    size_t size() const {
        return data.size();
    }

    void emplace(size_t id, timeval tv) {
        data.emplace_back(id, tv);
        std::push_heap(data.begin(), data.end(), cmp);
    }

    /**
     * @brief Updates the timeouts in the queue by subtracting the given timeval.
     * 
     * This function iterates through all elements in the queue and
     * subtracts the given timeval from each element's timeout.
     * 
     * @param tv The timeval to subtract from each element's timeout.
     * @note This function requires to call `handle_timeouts` afterwards.
     */
    void update_timeouts(timeval tv) {
        const time_t sec = tv.tv_sec;
        const suseconds_t usec = tv.tv_usec;

        for (auto &el : data) {
            auto &eltv = el.second;

            eltv.tv_sec -= sec;
            eltv.tv_usec -= usec;

            if (eltv.tv_usec < 0) {
                eltv.tv_sec -= 1;
                eltv.tv_usec += 1000000;
            }
        }
    }

    /**
     * @brief Removes all expired timeouts from the queue.
     *
     * Iterates through the priority queue and removes all
     * elements whose timeout has expired.
     * 
     * @note This function should be called after handling all
     *       expired timeouts to keep the queue up to date and
     *       prevent processing the same timeout multiple times.
     */
    void clear_timeouts() {
        while (!empty() && is_timeout_expired(top().second)) {
            pop();
        }
    }

    /**
     * @brief Executes the given function for each element in the queue.
     * @param f The function to apply to each element.
     * @note Function `f` should not change the queue structure.
     */
    template<typename Func>
    void for_each(Func f) const {
        for (auto& el : data)
            f(el);
    }

    /**
     * @brief Executes a function for each element in the queue while a predicate is true.
     *
     * Iterates over all elements in the queue. For each element, the predicate `pred` is called.
     * If `pred(element.second)` returns true, the function `f` is executed for that element.
     * Iteration stops at the first element for which the predicate returns false.
     *
     * @tparam Predicate Callable returning bool, used to test each element.
     * @tparam Func Callable to execute for each element while the predicate is true.
     * @param pred Predicate function to test each element's timeval structure.
     * @param f Function to apply to each element while pred(element) is true.
     *
     * @note The function `f` should not modify the structure of the queue.
     */
    template<typename Predicate, typename Func>
    void for_each_while(Predicate pred, Func f) const {
        for (auto &el : data) {
            if (!pred(el.second)) break;
            f(el);
        }
    }
};

/** Priority queue of timeouts for clients. */
ModifiableIdxTvPriorityQueue timeouts;

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
        // TODO Czy należy coś robić z conn (it->second)?
        connections.erase(it);
    }
    
    close(poll_descriptors.at(idx).fd);
    
    poll_descriptors.at(idx).fd = -1;
    free_poll_indices.push(idx);
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
     * @param idx The index of the client in the poll descriptors.
     * @param rinfo Information about the received message.
     * 
     * @note This function assumes, that
     *       tga::MessageCombinators::deserialize_message()
     *       function resulted in a successful deserialization, meaning
     *       the message type is corrected and
     *       all message values are VALID, BUT NOT CORRECT (see PutHandler::handle).
     */
    virtual void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) = 0;
};

class HelloHandler : public MessageHandler {
public:
    /**
     * @brief Handles the HELLO message from the client.
     * 
     * This function checks if the client is already baptised. If not, it baptises
     * the client with the given ID and registers the client in the clients manager.
     * Otherwise, it ignores the message and sets an error in the receive info.
     * 
     * @param msg The received HELLO message.
     * @param idx The index of the client in the poll descriptors.
     * @param rinfo Information about the received message.
     */
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        ClientConnection &conn = connections.at(idx);
        HelloMessage hello_msg = dynamic_cast<HelloMessage &>(*msg);

        if (conn.baptised()) {
            tga::io::log::err::message(hello_msg.serialize(),
                                    conn.get_client_id(),
                                    conn.get_addr());
            rinfo.err = ReceivedDataStatus::INVALID_TYPE;
        } else {
            // Baptise client with given ID.
            conn.baptise(hello_msg.getPlayerID());
            tga::io::log::info::server::client_known(
                tga::net::get_ip(conn.get_addr()),
                tga::net::get_port(conn.get_addr()),
                conn.get_client_id());
            
            // Send Coeff
            // TODO Implement
        }
    }
};

class CoeffHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        (void) idx; // Unused parameter
        // TODO Implement

    }
};

class PutHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        ClientConnection &conn = connections.at(idx);
        PutMessage put_msg = dynamic_cast<PutMessage &>(*msg);

        const size_t point = put_msg.getPoint();
        const double value = (double) put_msg.getValue();

        // Check, if point or value is out of range.
        if (point > tga::args::server::K() || value < -5.0 || value > 5.0) {
            rinfo.err = ReceivedDataStatus::INVALID_VALUE;
            tga::io::log::err::message(
                put_msg.serialize(),
                conn.get_client_id(),
                conn.get_addr());
            
            // Plan sending BAD_PUT Message.
            MsgPtr timeout_msg = std::make_unique<BadPutMessage>(
                                        point, put_msg.getValue());
            conn.setTimeoutMeaning(TimeoutMeaning::SEND_DELAY);
            conn.setTimeoutMessage(std::move(timeout_msg));
            timeouts.emplace(idx, bad_put_delay);
        } else { // Correct PUT message.
            conn.approxAdd(point, value);
            // TODO Napisać taki log
            // tga::io::log::info::server::put(
            //     conn.get_client_id(),
            //     conn.get_addr(),
            //     point,
            //     value);
            
            // Plan sending STATE Message.
            MsgPtr timeout_msg = std::make_unique<StateMessage>(conn.getApproximations());
            conn.setTimeoutMeaning(TimeoutMeaning::SEND_DELAY);
            conn.setTimeoutMessage(std::move(timeout_msg));
            timeouts.emplace(idx, conn.getStateDelay());
        }
    }
};

class BadPutHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        (void) idx; // Unused parameter
        // TODO Implement
    }
};

class StateHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        (void) idx; // Unused parameter// TODO Implement

    }
};

class PenaltyHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        (void) idx; // Unused parameter
        // TODO Implement
    }
};

class ScoringHandler : public MessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        (void) idx; // Unused parameter
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

/**
 * @brief Handles a message received from a client.
 *
 * This function processes a message received from a client at the specified index in the poll descriptors.
 * It attempts to deserialize the message from the client's buffer and performs validation and error handling.
 * If the message is invalid or deserialization fails, it logs the error, optionally closes the connection
 * (e.g., if the client is not yet baptised), and returns. If the message is valid, it ensures protocol correctness
 * (e.g., the first message from a new client must be a valid HELLO message), and if so, dispatches the message
 * to the appropriate handler based on its type.
 *
 * The function also updates the ReceiveInfo structure with error information if deserialization or validation fails.
 * It is responsible for enforcing protocol rules, such as requiring a HELLO message as the first message from a client,
 * and for invoking the correct message handler for further processing of valid messages.
 *
 * @param idx Index of the client in the poll_descriptors vector.
 * @param len_received Number of bytes received from the client.
 * @param rinfo Reference to a ReceiveInfo structure where error information will be stored.
 *
 * @note If the message is invalid or the protocol is violated, the function may close the client connection.
 * @note This function should be called after data has been read from the client's socket into its buffer.
 * @note The function logs all invalid messages and protocol violations for auditing and debugging purposes.
 */
void handle_received_message(const size_t idx, const size_t len_received, ReceiveInfo &rinfo) {
    assert(len_received > 0);

    const char *buffer = connections.at(idx).get_buffer();
    MsgPtr msg = tga::msg::deserialize_message(buffer, len_received, rinfo);
    const auto &conn = connections.at(idx);

    
    if (msg == nullptr || rinfo.err != ReceivedDataStatus::SUCCESS) { // Invalid message.
        std::string message_text(buffer, len_received);
        auto player_id = conn.baptised() ? conn.get_client_id() : "UNKNOWN";
        auto addr = conn.get_addr();
        tga::io::log::err::message(message_text, player_id, addr);
        
        if (!conn.baptised()) close_client_connection(idx);
    } else { // Valid message.
        if (tga::config::server) {
            // First message from the client MUST BE a correct HELLO message.
            // Otherwise, the connection with the client has to be closed.
            if (!conn.baptised() && msg->messageType() != "HELLO") {
                tga::io::log::err::message(
                    msg->serialize(),
                    conn.get_client_id(),
                    conn.get_addr());
                close_client_connection(idx);
                return;
            }
        }

        MsgHandlerPtr handler = make_handler(msg->messageType());
        handler->handle(msg, idx, rinfo);
    }
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

/**
 * @brief Updates all client timeouts by the elapsed time since last call.
 *
 * This function calculates the time difference since the last call
 * and subtracts it from the timeout values of all clients in the priority queue.
 * It should be called periodically (e.g., once per event loop iteration)
 * to ensure that client timeouts are properly decremented as time passes.
 *
 * @note This function does not handle expired timeouts itself - after calling it,
 *       you should call a function that processes clients whose timeouts have expired
 *       (i.e. `handle_timeouts()`).
 */
void update_timeouts() {
    timeval current_time;
    if (gettimeofday(&current_time, nullptr) < 0) {
        tga::io::log::err::error("gettimeofday");
        exit(1);
    }

    // Initialize `previous_time` on first call.
    static timeval previous_time = current_time;

    timeval diff = {
        previous_time.tv_sec - current_time.tv_sec,
        previous_time.tv_usec - current_time.tv_usec
    };
    timeouts.update_timeouts(diff);

    previous_time = current_time;
}

// NOTE Should be called after `update_timeouts`.
void handle_timeouts() {
    // Iteruj przez wszystkie elementy, których timeout minął.
    
    auto _handle_timeout = [](const std::pair<size_t, timeval> &el) {
        size_t idx = el.first;

        auto &conn = connections.at(idx);
        auto timeout_meaning = conn.getTimeoutMeaning();

        if (timeout_meaning == TimeoutMeaning::NO_MESSAGE_RECEIVED) {
            // TODO Napisać taki log
            // tga::io::log::server::info::timeout(
            //     conn.get_client_id(),
            //     conn.get_addr(),
            //     tv);
            
            /** NOTE
             * For now the only possibility of timeout meaning the expected
             * message did not arrive is when the client did not send HELLO message
             * within the expected time after establishing connection, to which
             * server responds with closing the connection with the client.
             */
            close_client_connection(idx);
        } else if (timeout_meaning == TimeoutMeaning::SEND_DELAY) {
            // Send the delayed message.
            auto msg = conn.getTimeoutMessage();
            assert(msg.has_value()); // Should not be empty, if timeout meaning is SEND_DELAY.
            
            if (msg) {
                // TODO Napisać taki log, jeśli jest wymagany
                // tga::io::log::info::server::send_message(
                //     msg->serialize(),
                //     conn.get_client_id(),
                //     conn.get_addr());
                // TODO Send the message to the client.
            }
        }
    };

    timeouts.for_each_while(is_timeout_expired, _handle_timeout);
    timeouts.clear_timeouts(); // Clear the expired timeouts from the queue.
}

/**
 * @brief Checks if a connection exists for the given index.
 * @param idx The index of the connection in the poll descriptors.
 * @return true if the connection exists, false otherwise.
 * @note This function assumes, that `idx` is a valid index in `poll_descriptors`.
 */
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
        timeouts.emplace(idx, hello_timeout);
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
        ssize_t len_received = read(poll_descriptors.at(idx).fd, connections.at(idx).get_buffer(), 0);
        if (len_received < 0) tga::io::log::err::error("read from existing connection");
        else if (len_received == 0) close_client_connection(idx);
        else handle_received_message(idx, (size_t) len_received, rinfo);
    }

    if ((poll_descriptors.at(idx).revents & POLLOUT) != 0) {
        // TODO Implement
    }
}

/**
 * @brief Polls in-out events from the clients.
 */
int poll_events() {
    int timeout = -1;
    if (!timeouts.empty()) {
        const auto &tv = timeouts.top().second;
        timeout = tv.tv_sec * 1000 + tv.tv_usec / 1000; // Convert to miliseconds.
    }
    return poll(poll_descriptors.data(),
            (nfds_t) poll_descriptors.size(),
            timeout);
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