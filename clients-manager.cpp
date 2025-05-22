/**
 * This file stores all the important information about clients.
 * It can be used by server only.
 */

#include <map>
#include <string>
#include <netinet/in.h>
#include <variant>
#include <cstring>

#include "clients-manager.h"
#include "utils.h"

using tga::utils::CommunicationPhase;

namespace tga {
namespace cltman {

namespace {

/**
 * @brief Structure allowing client identification.
 * This structure contains the client ID and the address.
 */
class Client {
private:
    enum AddrVariantIndex {
        IDX_NONE = 0,
        IDX_IPV4 = 1,
        IDX_IPV6 = 2
    };

    std::string id; // Client identificator.
    std::variant<std::monostate, sockaddr_in, sockaddr_in6> addr; // Address of the client.

public:
    // Constructors
    Client() : addr(std::monostate{}) {}
    Client(const std::string& id, const sockaddr_in& addr)
        : id(id), addr(addr) {}
    Client(const std::string& id, const sockaddr_in6& addr)
        : id(id), addr(addr) {}

    /**
     * @brief Comparison operator for Client.
     * This operator compares two Client objects based on their ID and address.
     * It is used to check if two clients are the same.
     * @param other The other Client object to compare with.
     * @return true if the clients are equal, false otherwise.
     */
    bool operator==(const Client& other) const {
        if (id != other.id) return false;
        if (addr.index() != other.addr.index()) return false;
        if (addr.index() == IDX_IPV4) {
            const sockaddr_in& ipv4_addr = std::get<sockaddr_in>(addr);
            const sockaddr_in& other_ipv4_addr = std::get<sockaddr_in>(other.addr);
            return ipv4_addr.sin_addr.s_addr == other_ipv4_addr.sin_addr.s_addr;
        } else if (addr.index() == IDX_IPV6) {
            const sockaddr_in6& ipv6_addr = std::get<sockaddr_in6>(addr);
            const sockaddr_in6& other_ipv6_addr = std::get<sockaddr_in6>(other.addr);
            return std::memcmp(ipv6_addr.sin6_addr.s6_addr,
                               other_ipv6_addr.sin6_addr.s6_addr,
                               sizeof(ipv6_addr.sin6_addr.s6_addr)
                            ) == 0;
        }

        return true; // Family not set in both, thus equal.
    }

    /**
     * @brief Less than operator for Client.
     * This operator compares two Client objects based on their ID and address.
     * It is used to store clients in a map.
     * @param other The other Client object to compare with.
     * @return true if this client is less than the other, false otherwise.
     */
    bool operator<(const Client& other) const {
        if (id != other.id) return id < other.id;
        
        // Those does not matter, but we need to differentiate them in std::map.
        if (addr.index() != other.addr.index()) {
            return addr.index() < other.addr.index(); // IPv4 < IPv6.
        } else if (addr.index() == IDX_IPV4) {
            const sockaddr_in& ipv4_addr = std::get<sockaddr_in>(addr);
            const sockaddr_in& other_ipv4_addr = std::get<sockaddr_in>(other.addr);
            return ipv4_addr.sin_addr.s_addr < other_ipv4_addr.sin_addr.s_addr;
        } else if (addr.index() == IDX_IPV6) {
            const sockaddr_in6& ipv6_addr = std::get<sockaddr_in6>(addr);
            const sockaddr_in6& other_ipv6_addr = std::get<sockaddr_in6>(other.addr);
            return std::memcmp(ipv6_addr.sin6_addr.s6_addr,
                               other_ipv6_addr.sin6_addr.s6_addr,
                               sizeof(ipv6_addr.sin6_addr.s6_addr)
                            ) < 0;
        } else {
            return false; // This case should not happen, since there should not exist
                          // two clients with the same ID and undefined family.
        }
    }
};

/**
 * @brief Information about a client.
 * This class contains the communication phase and the number of
 * approximations for each of the 9 digits.
 */
class ClientInfo {
private:
    CommunicationPhase phase;
    unsigned approximations[9] = {0};
};

/**
 * @brief Map of clients and their information.
 * The key is the client ID, and the value is the client information.
 */
std::map<Client, ClientInfo> clients;

/**
 * @brief Number of correct client requests handled since
 * the start of the program. Less than or equal M (see README).
 */
unsigned requests_handled_count = 0;

} // anonymous namespace

/**
 * @brief Retrieves the total number of correct client requests handled.
 * @return The number of correct client requests handled.
 */
unsigned requests_handled() { return requests_handled_count; }

} // namespace cltman
} // namespace tga