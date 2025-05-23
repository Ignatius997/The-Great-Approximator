/**
 * This file stores all the important information about clients.
 * It can be used by server only.
 */

#include <map>
#include <string>
#include <netinet/in.h>
#include <cstring>

#include "clients-manager.h"
#include "utils.h"
#include "netutils.h"
#include "communication.h"

using tga::comm::SockAddrVariant;
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
    const std::string id; // Client identificator.
    const SockAddrVariant addr; // Address of the client.

public:
    // Constructors
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
        return addr == other.addr;
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
        return addr < other.addr;
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