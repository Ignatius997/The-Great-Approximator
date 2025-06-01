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

using SockAddrVariant = std::variant<sockaddr_in, sockaddr_in6>;

/**
 * @brief This enum determines in which phase is currently
 * the communication between client and server.
 */
enum CommunicationPhase {
    PRE_GAME, // Already connected, waiting for HELLO message.
    WAITING_FOR_PUT,  // HELLO received, game is in progress, waiting for PUT messages.
    SENDING_PUT_RESPONSE, // PUT received, sending response to the client.
    END // Game is over, sending SCORING message to the client.
};

enum ReceivedDataStatus {
    SUCCESS,        // Message is valid and processed successfully.
    DISCONNECTED,   // Client/server disconnected.
    INVALID_TYPE,   // Unresolved or not the expected message type.
    INVALID_FORMAT, // Wrong format for the message type
    INVALID_VALUE   // Invalid value in the message data.
};

// FIXME Jeśli nie będzie tutaj innych wartości, to ReceiveInfo można zamienić na enum z wartościami ReceivedDataStatus.
/**
 * @brief This class is used to store information about the
 * received message.
 */
class ReceiveInfo {
public:
    ReceivedDataStatus err = SUCCESS; // Error of the message processing.
};

int _bind(int &sockfd, uint16_t port, const int family);
uint16_t extract_port(const int sockfd, const int family);

std::string get_ip(const SockAddrVariant &addr);
uint16_t get_port(const SockAddrVariant &addr);

} // namespace net
} // namespace tga

#endif // TGA_NUTIL_H