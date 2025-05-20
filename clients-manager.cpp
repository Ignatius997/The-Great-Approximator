/**
 * This file stores all the important information about clients.
 * It can be used by server only.
 */

#include <map>
#include <string>
#include <netinet/in.h>

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
    std::string id;
    sockaddr_in ipv4_addr;
    sockaddr_in6 ipv6_addr;
    int family; // AF_INET or AF_INET6. -1 if not set.

public:
    // Constructors
    Client() : family(-1) {}
    Client(const std::string& id, const sockaddr_in& addr)
        : id(id), ipv4_addr(addr), family(AF_INET) {}
    Client(const std::string& id, const sockaddr_in6& addr)
        : id(id), ipv6_addr(addr), family(AF_INET6) {}

    /**
     * @brief Comparison operator for Client.
     * This operator compares two Client objects based on their ID and address.
     * It is used to check if two clients are the same.
     * @param other The other Client object to compare with.
     * @return true if the clients are equal, false otherwise.
     */
    bool operator==(const Client& other) const {
        if (id != other.id) return false;
        if (family != other.family) return false;
        if (family == AF_INET) {
            return ipv4_addr.sin_addr.s_addr == other.ipv4_addr.sin_addr.s_addr;
        } else if (family == AF_INET6) {
            return ipv6_addr.sin6_addr.s6_addr == other.ipv6_addr.sin6_addr.s6_addr;
        } else {
            return true; // Family not set in both, thus equal.
        }
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
        if (family != other.family) return family < other.family;
        if (family == AF_INET) {
            return ipv4_addr.sin_addr.s_addr < other.ipv4_addr.sin_addr.s_addr;
        } else if (family == AF_INET6) {
            return ipv6_addr.sin6_addr.s6_addr < other.ipv6_addr.sin6_addr.s6_addr;
        } else {
            return false; // This case should not happen, since there should not be
                          // two clients with the same ID and not defined family.
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