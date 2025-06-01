#include <unistd.h>
#include <poll.h>
#include <sys/time.h>
#include <errno.h>

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
#include <cmath>

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

namespace {

constexpr size_t buffer_size = 1 << 13; // 8 KiB
constexpr size_t max_msg_len = buffer_size >> 1; // TODO Zastanowić się nad tym.
constexpr size_t min_len_to_read = max_msg_len >> 4;

// NOTE Added just out of pure vanity.
class MessageHandler {
public:
    virtual ~MessageHandler() = default;
};

} // anonymous namespace

namespace server {

namespace {

/** Time to wait for HELLO message
 * from the client from the moment of connection.
 */
constexpr timeval hello_timeout { .tv_sec = 3, .tv_usec = 0};

/** Delay for sending BAD_PUT. */
constexpr timeval bad_put_delay { .tv_sec = 1, .tv_usec = 0 };

const std::string empty_string = ""; // Empty string for client ID.

std::vector<std::vector<char>> buffers; // Buffers for receiving messages from clients.

constexpr int socket_queue_len = 10; // FIXME Ulepszyć to?
uint16_t port; // Stored in host byte order.
int ipv4_socket_fd = -1;
int ipv6_socket_fd = -1;

/**
 * @brief Poll descriptors for the clients.
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
    size_t msg_start_idx = 0; // Index of the start of the current message in the buffer.
    size_t buffer_len = 0; // Length of the buffer, i.e., how many valid bytes are currently in the buffer. Used for sending messages.
    size_t buffer_pos = 0; // Position in the buffer, i.e., how many bytes were already written to / read from the buffer.

    size_t penalty = 0; // Penalty for the client.

    std::optional<TimeoutMeaning> timeout_meaning = std::nullopt; // Meaning of the timeout, if it occurred.
    std::optional<MsgPtr> timeout_message = std::nullopt; // Message to send after the timeout, if it occurred.

    CommunicationPhase phase = CommunicationPhase::PRE_GAME;

    // FIXME Magiczna stała
    Rational coeffs[9] = {Rational("0")}; // Coefficients of the polynomial
    // FIXME Po pierwsze magiczna stała
    // FIXME Po drugie, to czy nie można tego zrobić w wektorze?
    Rational client_approximations[10001] = {Rational("0")}; // Client's approximations for the polynomial, indexed by point (0 to 10000).
    Rational server_approximations[10001] = {Rational("0")}; // Server's approximations for the polynomial, indexed by point (0 to 10000).

    /**
     * @brief Calculates the value of the polynomial at the given point idx.
     * @param idx The point index (0 to K).
     * @return The value of the polynomial at the given point as a Rational number.
     */
    Rational calculate_function_value(size_t x) const {
        // Simply returns f(x) = c_0 + c_1 * x + c_2 * x^2 + ... + c_N * x^N
        Rational result = Rational("0");
        for (size_t exp = 0; exp <= tga::args::server::N(); ++exp) {
            result += coeffs[exp] * Rational(std::pow(static_cast<double>(x),
                                                    static_cast<double>(exp)));
        }
        return result;
    }

public:
    ClientConnection(const SockAddrVariant &addr) : addr(addr) {}

    /**
     * @brief Get the client ID.
     * @return The client ID as a string.
     */
    [[nodiscard]]
    std::string get_client_id() const {
        // NOTE Czy powinno zwracać UNKNOWN w przypadku braku ID? ODP Nie
        return client_id;
    }

    /**
     * @brief Get the address of the client.
     * @return The address of the client as a SockAddrVariant.
     */
    [[nodiscard]]
    SockAddrVariant get_addr() const { return addr; }

    /**
     * @brief Get the communication phase of the client.
     * @return The communication phase of the client.
     */
    [[nodiscard]]
    CommunicationPhase get_phase() const { return phase; }

    /**
     * @brief Sets the communication phase for the client.
     * @param new_phase The new communication phase.
     */
    void set_phase(const CommunicationPhase new_phase) {
        assert(new_phase != CommunicationPhase::PRE_GAME);
        phase = new_phase;

        // Just in case.
        buffer_pos = 0;
        buffer_len = 0;
    }

    /**
     * @brief Give name to the already connected, but unnamed client.
     */
    void baptise(const std::string &name) {
        assert(phase == CommunicationPhase::PRE_GAME);
        client_id = name;
        phase = CommunicationPhase::WAITING_FOR_PUT;
    }

    /**
     * @brief Returns pointer to the client's buffer.
     * @return pointer to the client's buffer.
     */
    [[nodiscard]]
    char *get_buffer() {
        return buffer;
    }

    /**
     * @brief Returns the length of the client's buffer,
     *        i.e. number of valid bytes in the buffer.
     * @return The length of the client's buffer.
     */
    [[nodiscard]]
    size_t get_buffer_len() const {
        return buffer_len;
    }

    /**
     * @brief Sets the length of the client's buffer.
     * @param len The new length of the client's buffer.
     */
    void set_buffer_len(size_t len) {
        assert(len <= buffer_size);
        buffer_len = len;
    }

    /**
     * @brief Returns the position in the client's buffer,
     *        i.e. how many bytes were already written to the buffer.
     * @return The position in the client's buffer.
     */
    [[nodiscard]]
    size_t get_buffer_pos() const {
        return buffer_pos;
    }

    /**
     * @brief Sets the position in the client's buffer.
     * @param pos The new position in the client's buffer.
     */
    void set_buffer_pos(size_t pos) {
        assert(pos <= buffer_len);
        buffer_pos = pos;
    }

    /**
     * @brief Returns index of the start of the current message in the buffer.
     * @return The index of the start of the current message in the buffer.
     */
    [[nodiscard]]
    size_t get_msg_start_idx() const {
        return msg_start_idx;
    }

    /**
     * @brief Sets the index of the start of the current message in the buffer.
     * @param idx The new index of the start of the current message in the buffer.
     */
    void set_msg_start_idx(size_t idx) {
        assert(idx < buffer_size);
        msg_start_idx = idx;
    }

    std::vector<Rational> get_coeffs() const {
        assert(phase != CommunicationPhase::PRE_GAME);
        return std::vector<Rational>(coeffs, coeffs + tga::args::server::N() + 1);
    }

    /**
     * @brief Sets the coefficients of the polynomial for the client.
     * @param new_coeffs The coefficients to set.
     */
    void set_coeffs(std::vector<Rational> new_coeffs) {
        // assert(phase == CommunicationPhase::PRE_GAME); // FIXME This is actually not true
        assert(new_coeffs.size() == tga::args::server::N() + 1);
        for (size_t i = 0; i <= tga::args::server::N(); ++i) {
            coeffs[i] = new_coeffs[i];
        }
    }

    /**
     * @brief Adds a value to the approximation for the given point.
     * @param point The point index (0 to 10000).
     * @param value The value to add to the approximation (-5.0 to 5.0).
     * @note The function assumes, that point <- 10^4 and |value| <= 5.0.
     */
    void approx_add(size_t point, double value) {
        assert(point <= 10000);
        assert(value >= -5.0 && value <= 5.0);
        client_approximations[point] += Rational(value);
    }

    /**
     * @brief Sets the meaning of the timeout that occurred for the client.
     * @param meaning The meaning of the timeout.
     */
    void set_timeout_meaning(const TimeoutMeaning meaning) {
        timeout_meaning = meaning;
    }

    /**
     * @brief Returns the meaning of the timeout that occurred for the client.
     * @return The meaning of the timeout, or std::nullopt if no timeout occurred.
     */
    [[nodiscard]]
    std::optional<TimeoutMeaning> get_timeout_meaning() const {
        return timeout_meaning;
    }

    /**
     * @brief Sets the message to send after the timeout.
     * @param msg The message to send after the timeout.
     * @note `msg` is moved into the object, so it should not be used after this call.
     */
    void set_timeout_message(MsgPtr msg) {
        timeout_message = std::move(msg);
    }

    /**
     * @brief Returns the message to send after the timeout.
     * @return The message to send after the timeout, or std::nullopt if no message is set.
     * @note The message is moved from the object, so it should not be used after this call.
     */
    [[nodiscard]]
    std::optional<MsgPtr> get_timeout_message() {
        auto msg = std::move(timeout_message);
        timeout_message = std::nullopt; // Clear the message after moving it.
        return msg;
    }

    std::vector<Rational> get_client_approximations() const {
        auto end = client_approximations + tga::args::server::K();
        return std::vector<Rational>(client_approximations, end);
    }

    timeval get_state_delay() const {
        assert(phase != CommunicationPhase::PRE_GAME);

        static time_t lowercase_count = std::count_if(
            client_id.begin(), client_id.end(),
            [](unsigned char c) { return std::islower(c); }
        );
        static timeval tv = {
            .tv_sec = lowercase_count,
            .tv_usec = 0
        };
        return tv;
    }

    /**
     * @brief Increases the penalty for the client by given amount.
     * @param pen The count of penalty points to impose.
     * @note For now the only time, when penalty is imposed, is when PUT
     *       is sent in a wrong phase of communication.
     */
    void impose_penalty(const size_t pen) { penalty += pen; }

    /**
     * @brief Calculates the client's score.
     *
     * Computes the score as the sum of squared differences between the client's
     * and server's approximations, plus the penalty. Assumes all server
     * approximations are calculated and the phase is END.
     *
     * @return The calculated score as a Rational value.
     */
    [[nodiscard]]
    Rational calculate_score() {
        /** TODO
         * Assert, that all server approximations are calculated.
         * Here is a placeholder implementation
         */
        assert(phase == CommunicationPhase::END);

        for (size_t i = 0; i <= tga::args::server::K(); ++i) {
            server_approximations[i] = calculate_function_value(i);
        }

        // Calculate the score as the sum of squared differences.
        Rational score = Rational("0");
        for (size_t i = 0; i <= tga::args::server::K(); ++i) {
            Rational diff = client_approximations[i] - server_approximations[i];
            score += diff * diff;
        }

        // Add penalty to the score.
        score += Rational(static_cast<double>(penalty));

        return score;
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

    // Allow only IPv6 connections if family is AF_INET6.
    if (family == AF_INET6) {
        int opt = 1;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
            tga::io::log::err::error("setsockopt IPV6_V6ONLY");
            exit(1);
        }
    }

    if (tga::net::_bind(sockfd, port, family) == 1) {
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
    if (tga::config::debug) {
        // TODO Napisać oddzielny log do zamykania połączeń
        auto player_id = connections.at(idx).get_client_id() == empty_string ? 
            "UNKNOWN" : connections.at(idx).get_client_id();
        tga::io::log::info::custom("Closing connection with " + player_id + "-[" +
            tga::net::get_ip(connections.at(idx).get_addr()) + "]:" +
            std::to_string(tga::net::get_port(connections.at(idx).get_addr())));
    }
    
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
    assert(connections.find(idx) == connections.end());
    connections.emplace(idx, ClientConnection(client_addr));
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
        client_addr = sockaddr_in{}; // Initialize to IPv4 address.
        addr = (sockaddr *) (&std::get<sockaddr_in>(client_addr));
        addr_len = (socklen_t) sizeof(sockaddr_in);
    } else {
        sockfd = ipv6_socket_fd;
        client_addr = sockaddr_in6{}; // Initialize to IPv6 address.
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

/**
 * @brief Prepares a message to be sent to the client.
 * 
 * This function prepares a message to be sent to the client by copying
 * the serialized message into the client's buffer and setting the
 * appropriate poll descriptor events.
 * 
 * @param idx The index of the client in the poll descriptors.
 * @param msg The message to send to the client.
 */
void prepare_to_send(const size_t idx, MsgPtr msg) {
    // Create some useful aliases for readability
    ClientConnection &conn = connections.at(idx);
    const std::string serialized_msg = msg->serialize();
    const size_t msg_len = serialized_msg.size();

    assert(conn.get_phase() != CommunicationPhase::PRE_GAME);
    assert(msg_len < buffer_size);

    // Switch to writing.
    poll_descriptors.at(idx).events = POLLOUT;

    // Prepare buffer
    std::memcpy(conn.get_buffer(), serialized_msg.data(), msg_len);
    conn.set_buffer_len(msg_len);
    conn.set_buffer_pos(0);
}

// NOTE Not tested.
/**
 * @brief Converts a string of coefficients into a vector of Rational numbers.
 * 
 * This function takes a string containing coefficients separated by spaces
 * and converts it into a vector of Rational objects.
 * 
 * @param coeffs_str The string containing coefficients in format of "$c_0 $c_1 ... $c_N".
 * @return A vector of Rational objects representing the coefficients.
 */
std::vector<Rational> convert_coeffs_string_to_vector(std::string coeffs_str) {
    std::vector<Rational> coeffs_vec;
    size_t start = 0;

    while (start < coeffs_str.size()) {
        size_t end = start;
        while (end < coeffs_str.size() && !std::isspace(coeffs_str[end])) ++end;
        coeffs_vec.emplace_back(coeffs_str.substr(start, end - start));
        start = end + 1; // Move to the next coefficient, skipping the space.
    }

    return coeffs_vec;
}

/**
 * @brief Returns reference to variable holding number of
 * correct messages left to receive from the clients.
 */
size_t& messages_to_receive() {
    static size_t count = tga::args::server::M();
    return count;
}

// ==== Message handlers ====

class ServerMessageHandler : public MessageHandler {
public:
    virtual ~ServerMessageHandler() = default; // TODO O co biega?

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

class HelloHandler : public ServerMessageHandler {
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

        if (conn.get_phase() != CommunicationPhase::PRE_GAME) {
            // Client is already baptised, but sent HELLO message.
            tga::io::log::err::server::message(hello_msg.serialize(),
                                    conn.get_client_id(),
                                    conn.get_addr());
            rinfo.err = ReceivedDataStatus::INVALID_TYPE;
            return; // Ignore the message
        }

        // Baptise client with given ID.
        conn.baptise(hello_msg.getPlayerID());
        tga::io::log::info::server::client_known(
            tga::net::get_ip(conn.get_addr()),
            tga::net::get_port(conn.get_addr()),
            conn.get_client_id());
        
        // Store coefficients and prepare for sending COEFF.
        std::string coeffs_str = tga::io::file::read_coeffs();
        auto coeffs_vec = convert_coeffs_string_to_vector(std::move(coeffs_str));
        conn.set_coeffs(std::move(coeffs_vec));
        auto coeff_msg = std::make_unique<CoeffMessage>(conn.get_coeffs());
        prepare_to_send(idx, std::move(coeff_msg));
    }
};

class PutHandler : public ServerMessageHandler {
public:
    void handle(const MsgPtr &msg, const size_t idx, ReceiveInfo &rinfo) override {
        ClientConnection &conn = connections.at(idx);
        PutMessage put_msg = dynamic_cast<PutMessage &>(*msg);

        const size_t point = put_msg.getPoint();
        const double value = (double) put_msg.getValue();

        // Check, if PUT message is sent in between
        // receiving earlier PUT and sending PUT response.
        if (conn.get_phase() != CommunicationPhase::WAITING_FOR_PUT) {
            tga::io::log::err::server::message(
                put_msg.serialize(),
                conn.get_client_id(),
                conn.get_addr());
            rinfo.err = ReceivedDataStatus::INVALID_TYPE;
            
            if (conn.get_phase() == CommunicationPhase::SENDING_PUT_RESPONSE) {
                // Prepare for sending PENALTY and impose a penalty.
                auto penalty_msg = std::make_unique<PenaltyMessage>(point, value);
                prepare_to_send(idx, std::move(penalty_msg));                
                conn.impose_penalty(20); // FIXME Magiczna stała
            }
        }

        // Check, if point or value is out of range.
        if (point > tga::args::server::K() || value < -5.0 || value > 5.0) {
            rinfo.err = ReceivedDataStatus::INVALID_VALUE;
            tga::io::log::err::server::message(
                put_msg.serialize(),
                conn.get_client_id(),
                conn.get_addr());
            
            // Plan sending BAD_PUT Message.
            MsgPtr timeout_msg = std::make_unique<BadPutMessage>(
                                        point, put_msg.getValue());
            conn.set_timeout_meaning(TimeoutMeaning::SEND_DELAY);
            conn.set_timeout_message(std::move(timeout_msg));
            timeouts.emplace(idx, bad_put_delay);
        } else { // Correct PUT message.
            // Check, if client is in the correct phase.
            if (conn.get_phase() != CommunicationPhase::WAITING_FOR_PUT) return;

            conn.approx_add(point, value);
            // TODO Napisać taki log
            // tga::io::log::info::server::put(
            //     conn.get_client_id(),
            //     conn.get_addr(),
            //     point,
            //     value);
            
            // Plan sending STATE Message.
            MsgPtr timeout_msg = std::make_unique<StateMessage>(
                                    conn.get_client_approximations());
            conn.set_timeout_meaning(TimeoutMeaning::SEND_DELAY);
            conn.set_timeout_message(std::move(timeout_msg));
            timeouts.emplace(idx, conn.get_state_delay());
        }
    }
};

using MsgHandlerPtr = std::unique_ptr<ServerMessageHandler>;

MsgHandlerPtr make_handler(const std::string &msg_type) {
    if (msg_type == "HELLO") {
        return std::make_unique<HelloHandler>();
    } else if (msg_type == "PUT") {
        return std::make_unique<PutHandler>();
    } else {
        tga::io::log::err::error("unknown message type: " + msg_type);
        return nullptr;
    }
}

/**
 * @brief Handles data received from a client.
 *
 * This function processes data received from a client at the specified index in the poll descriptors.
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
void handle_received_data(const size_t idx, const size_t len_received, ReceiveInfo &rinfo) {
    assert(len_received > 0);

    ClientConnection &conn = connections.at(idx);
    char *buffer = conn.get_buffer();
    size_t data_left = len_received; // Remaining data to process.

    while (data_left) {
        const size_t msg_start_idx = conn.get_msg_start_idx(); // Początek aktualnej wiadomości.
        
        std::string full_data(buffer + conn.get_buffer_pos(),
                            static_cast<size_t>(len_received));
        const size_t first_crlf_idx = full_data.find("\r\n");
        data_left = data_left - first_crlf_idx - 2; // -2 for '\r\n' itself.

        // new_pos: new buffer position; current_len: length of the current message.
        size_t new_pos, current_len;
        if (first_crlf_idx == std::string::npos) {
            new_pos = conn.get_buffer_pos() + len_received;
            current_len = new_pos - msg_start_idx; // Length of the current message.
        } else { // '\r\n' found, we can process the message.
            new_pos = conn.get_buffer_pos() + first_crlf_idx + 2; // +2 for '\r\n'.
            current_len = new_pos - msg_start_idx - 2;
        }

        if (first_crlf_idx == std::string::npos) { // '\r\n' not found.
            if (current_len < max_msg_len) { // Message too long or incorrect.
                rinfo.err = ReceivedDataStatus::INVALID_TYPE;
                tga::io::log::err::server::message(
                    std::string(buffer + msg_start_idx, current_len),
                    conn.get_client_id(),
                    conn.get_addr());
                
                conn.set_buffer_pos(new_pos);
                conn.set_msg_start_idx(new_pos);
            } else if (buffer_size - new_pos < min_len_to_read) { // Buffer is (almost) full.
                // Move the unprocessed data to the beginning of the buffer.
                std::memcpy(buffer, buffer + msg_start_idx, current_len);
                conn.set_buffer_pos(current_len);
                conn.set_msg_start_idx(0);
            } else {
                // Buffer is not full, but no complete message to process yet.
                conn.set_buffer_pos(new_pos);
            }

            break; // No complete message to process yet.
        }

        /** NOTE Zrobić takie wizualne przedstawienie do każdej części.
         * a b c \r \n d e f
         * 0 1 2 3  4  5 6 7
         * first_crlf_idx = 3
         * msg_start_idx = 0
         */

        MsgPtr msg = tga::msg::deserialize_message(buffer + msg_start_idx,
                                            first_crlf_idx - msg_start_idx + 2,
                                            rinfo);
        
        // FIXME Ale ten warunek brzydkko wygląda
        if (msg == nullptr || rinfo.err != ReceivedDataStatus::SUCCESS ||
                (conn.get_phase() == CommunicationPhase::PRE_GAME && 
                msg->messageType() != "HELLO")) { // Invalid message.
            std::string message_text(buffer, len_received);
            auto addr = conn.get_addr();
            std::string player_id = "UNKNOWN";
            if (conn.get_phase() != CommunicationPhase::PRE_GAME) {
                player_id = conn.get_client_id();
            }

            tga::io::log::err::server::message(message_text, player_id, addr);
            
            if (conn.get_phase() == CommunicationPhase::PRE_GAME) {
                // First message from the client MUST BE a correct HELLO message.
                close_client_connection(idx);
            }
        }

        // Message is valid.
        MsgHandlerPtr handler = make_handler(msg->messageType());
        handler->handle(msg, idx, rinfo);

        if (rinfo.err == ReceivedDataStatus::SUCCESS) {
            tga::io::log::info::server::received(
                msg->serialize(),
                conn.get_client_id(),
                conn.get_addr());
            --messages_to_receive();
        }
    }

    // Update the buffer position and message start index,
    // if possible without copying.
    if (conn.get_buffer_pos() == conn.get_msg_start_idx()) {
        conn.set_buffer_pos(0);
        conn.set_msg_start_idx(0);
    }
}

} // anonymous namespace

/** @brief Returns number of correct messages left to receive from the clients. */
size_t get_messages_to_receive() {
    return messages_to_receive(); // :)
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
        auto timeout_meaning = conn.get_timeout_meaning();

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
            auto msg = conn.get_timeout_message();
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
 * 
 * @note This function is called when a poll event occurs for SOME client,
 *       but maybe not for THIS client.
 */
void handle_poll_event(const size_t idx) {
    ReceiveInfo rinfo; // Object to store information about received message.
    pollfd &poll_fd = poll_descriptors.at(idx);

    // TODO To w końcu z POLLERR czy bez?
    if ((poll_fd.revents & (POLLIN | POLLERR)) != 0) {
        ClientConnection &conn = connections.at(idx);
        char *buffer = conn.get_buffer();
        size_t pos = conn.get_buffer_pos();

        ssize_t len_received = read(poll_fd.fd, buffer + pos, buffer_size - pos);
        if (len_received < 0) {
            tga::io::log::err::error("read from existing connection");
            close_client_connection(idx);
        } else if (len_received == 0) { // EOF, client disconnected.
            tga::io::log::info::server::client_disconnected(
                tga::net::get_ip(conn.get_addr()),
                tga::net::get_port(conn.get_addr()),
                conn.get_client_id());
                // TODO Można zoptymalizować jakoś to, że cały czas marnujemy
                // 3 linijki na get_ip, get_port, get_client_id
            rinfo.err = ReceivedDataStatus::DISCONNECTED;
            close_client_connection(idx);
        } else handle_received_data(idx, (size_t) len_received, rinfo);
    }

    if ((poll_fd.revents & POLLOUT) != 0) {
        ClientConnection &conn = connections.at(idx);
        const char *buffer = conn.get_buffer();
        size_t buffer_pos = conn.get_buffer_pos();
        size_t buffer_len = conn.get_buffer_len();

        ssize_t sent_bytes = write(poll_fd.fd,
                                buffer + buffer_pos,
                                buffer_len - buffer_pos);

        if (sent_bytes < 0) {
            tga::io::log::err::error("write to existing connection");
            if (errno != EINTR) { // Unless interrupted by a signal.
                close_client_connection(idx);
            }
        } else {
            conn.set_buffer_pos(buffer_pos + sent_bytes);

            if (conn.get_buffer_pos() == buffer_len) { // All data sent.
                tga::io::log::info::server::sent(
                    std::string(buffer, buffer_len),
                    conn.get_client_id(),
                    conn.get_addr());

                if (conn.get_phase() == CommunicationPhase::END) {
                    close_client_connection(idx); // TODO Coś jeszcze?
                    
                    if (connections.size() == 0) { // All clients disconnected.
                        // TODO Write some log about end of game.
                        // tga::io::log::info::server::all_clients_disconnected();
                        /** TODO
                         * W treści zadania jest napisane, że serwer
                         * "odczekuje 1 sekundę i rozpoczyna pracę od początku",
                         * jednak czy oznacza to, że ma spać?
                         */
                        sleep(1);
                    }

                } else {
                    poll_fd.events = POLLIN; // Switch to reading.
                    conn.set_phase(CommunicationPhase::WAITING_FOR_PUT);
                }
            }
        }
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
 * @brief Sends SCORING messages to clients.
 */
void send_scores() {
    std::map<std::string, Rational> scores_map;

    for (size_t idx = 0; idx < poll_descriptors.size(); ++idx) {
        if (connections.find(idx) != connections.end()) {
            ClientConnection &conn = connections.at(idx);
            conn.set_phase(CommunicationPhase::END);
            Rational score = conn.calculate_score();
            scores_map.at(conn.get_client_id()) = std::move(score);
        }
    }

    MsgPtr msg = std::make_unique<ScoringMessage>(scores_map);
    for (size_t idx = 0; idx < poll_descriptors.size(); ++idx) {
        if (connections.find(idx) != connections.end()) {
            prepare_to_send(idx, std::move(msg));
        }
    }
}

} // namespace server

namespace client {

namespace {

int socket_fd; // Socket file descriptor.
uint16_t port; // Stored in host byte order.
int family; // Address family (AF_INET or AF_INET6).

char buffer[buffer_size] = {0}; // Buffer for receiving and sending messages.
size_t buffer_pos = 0; // Current position in the buffer.
size_t msg_start_idx = 0; // Start index of the current message in the buffer.
// size_t buffer_len = 0; // Length of the current message in the buffer.

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

/**
 * @brief Closes connection with the server.
 * 
 * This function closes the socket file descriptor and resets it to -1.
 * It also logs the disconnection event.
 */
void close_connection() {
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
    
    // TODO Napisać taki log
    // tga::io::log::info::client::disconnected_from(tga::args::client::server(), port);
}

class ClientMessageHandler : public MessageHandler{
public:
    virtual ~ClientMessageHandler() = default;

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

class CoeffHandler : public ClientMessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement

    }
};

class BadPutHandler : public ClientMessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class StateHandler : public ClientMessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class PenaltyHandler : public ClientMessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

class ScoringHandler : public ClientMessageHandler {
public:
    void handle(const MsgPtr &msg, ReceiveInfo &rinfo) override {
        (void) msg; // Unused parameter
        (void) rinfo; // Unused parameter
        // TODO Implement
    }
};

using MsgHandlerPtr = std::unique_ptr<ClientMessageHandler>;

MsgHandlerPtr make_handler(const std::string &msg_type) {
    if (msg_type == "COEFF") {
        return std::make_unique<CoeffHandler>();
    } else if (msg_type == "STATE") {
        return std::make_unique<StateHandler>();
    } else if (msg_type == "SCORING") {
        return std::make_unique<ScoringHandler>();
    } else if (msg_type == "BAD_PUT") {
        return std::make_unique<BadPutHandler>();
    } else if (msg_type == "PENALTY") {
        return std::make_unique<PenaltyHandler>();
    } else {
        tga::io::log::err::error("unknown message type: " + msg_type);
        return nullptr;
    }
}

void handle_received_data(const ssize_t len_received, ReceiveInfo &rinfo) {
    assert(len_received > 0);

    size_t data_left = len_received; // Remaining data to process.

    while (data_left) {
        std::string full_data(buffer + buffer_pos,
                            static_cast<size_t>(len_received));
        const size_t first_crlf_idx = full_data.find("\r\n");
        data_left = data_left - first_crlf_idx - 2; // -2 for '\r\n' itself.

        // new_pos: new buffer position; current_len: length of the current message.
        size_t new_pos, current_len;
        if (first_crlf_idx == std::string::npos) {
            new_pos = buffer_pos + len_received;
            current_len = new_pos - msg_start_idx; // Length of the current message.
        } else { // '\r\n' found, we can process the message.
            new_pos = buffer_pos + first_crlf_idx + 2; // +2 for '\r\n'.
            current_len = new_pos - msg_start_idx - 2;
        }

        if (first_crlf_idx == std::string::npos) { // '\r\n' not found.
            if (current_len < max_msg_len) { // Message too long or incorrect.
                rinfo.err = ReceivedDataStatus::INVALID_TYPE;
                tga::io::log::err::client::message(
                    std::string(buffer + msg_start_idx, current_len));
                
                buffer_pos = new_pos;
                msg_start_idx = new_pos;
            } else if (buffer_size - new_pos < min_len_to_read) { // Buffer is (almost) full.
                // Move the unprocessed data to the beginning of the buffer.
                std::memcpy(buffer, buffer + msg_start_idx, current_len);
                buffer_pos = current_len;
                msg_start_idx = 0;
            } else {
                // Buffer is not full, but no complete message to process yet.
                buffer_pos = new_pos;
            }

            break; // No complete message to process yet.
        }

        /** NOTE Zrobić takie wizualne przedstawienie do każdej części.
         * a b c \r \n d e f
         * 0 1 2 3  4  5 6 7
         * first_crlf_idx = 3
         * msg_start_idx = 0
         */

        MsgPtr msg = tga::msg::deserialize_message(buffer + msg_start_idx,
                                            first_crlf_idx - msg_start_idx + 2,
                                            rinfo);
        
        if (msg == nullptr || rinfo.err != ReceivedDataStatus::SUCCESS) { // Invalid message.
            std::string message_text(buffer, len_received);
            tga::io::log::err::client::message(message_text);
        }

        // Message is valid.
        MsgHandlerPtr handler = make_handler(msg->messageType());
        handler->handle(msg, rinfo);

        if (rinfo.err == ReceivedDataStatus::SUCCESS) {
            tga::io::log::info::client::received(msg->serialize());
        }
    }

    // Update the buffer position and message start index,
    // if possible without copying.
    if (buffer_pos == msg_start_idx) {
        buffer_pos = 0;
        msg_start_idx = 0;
    }
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
 * @brief Receives a message from the server.
 * 
 * This function reads a message from the server and returns it as a
 * ReceiveInfo object.
 * 
 * @return The received message as a ReceiveInfo object.
 */
void receive_message() {  
    ReceiveInfo rinfo;

    ssize_t len_received = read(socket_fd, buffer, buffer_size);
    if (len_received < 0) {
        tga::io::log::err::error("read");
        close_connection();
    } else if (len_received == 0) { // EOF, server disconnected
        rinfo.err = ReceivedDataStatus::DISCONNECTED;
        close_connection();
    } else handle_received_data(len_received, rinfo);

    // TODO I co, i tyle? Chyba trzeba sprawdzić rinfo.
}
    
// NOTE Tutaj jest czysty MsgPtr, ale może warto by tu uwzględnić inne rzeczy, jeśli trzeba ofc
void send_message(MsgPtr msg) {
    std::string serialized_message = msg->serialize();

    size_t to_send = serialized_message.size();
    
    while (to_send > 0) {
        ssize_t len_sent = write(socket_fd, serialized_message.c_str(), serialized_message.size());

        if (len_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                len_sent = 0; // Not able to write right now, try later.
            }
            else {
                tga::io::log::err::error("write");
                close_connection();
                exit(1); // FIXME Is this safe? Should we maybe `return`?
            }
        }
        else {
            to_send -= (size_t)len_sent;
            // TODO Czy powinniśmy coś odczekać, żeby nie zapchać tego gniazda.
        }
    }

    tga::io::log::info::client::sent(serialized_message);
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

// ReceiveInfo receive_message() {
//     if (tga::config::server) return tga::comm::server::receive_message();
//     return tga::comm::client::receive_message();
// }

} // comm
} // tga