/**
 * This file stores all the important information about clients.
 * It can be used by server only.
 */

#include <map>
#include <string>
#include <netinet/in.h>

#include "utils.h"

using tga::utils::CommunicationPhase;

namespace tga {
namespace clients_man {

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

} // anonymous namespace

} // namespace clients_man
} // namespace tga