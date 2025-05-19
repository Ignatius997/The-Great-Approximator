#include <string>
#include <arpa/inet.h>
#include <iostream>

#include "err.h"

namespace tga {
namespace err {

// TODO Needs testing.
// FIXME Make it thread-safe.

void error(const std::string &error_description) {
    std::cerr << "ERROR: " << error_description << std::endl;
}

void message(const std::string &message_type, const std::string &player, const struct sockaddr_in &addr) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, INET_ADDRSTRLEN);
    uint16_t port = ntohs(addr.sin_port);

    std::cerr << "ERROR: bad message from [" << ip_str << "]:" << port
              << ", " << player << ": " << message_type << std::endl;
}

} // namespace err
} // namespace tga
