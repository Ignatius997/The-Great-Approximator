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

class Client {
    std::string id;
    sockaddr_in addr;
};

class ClientInfo {
private:
    CommunicationPhase phase;
    unsigned approximations[9] = {0};
};

std::map<Client, ClientInfo>;

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