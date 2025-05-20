#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#include "log.h"

// NOTE Needs testing.
// FIXME Make it thread-safe.

namespace tga {
namespace log {
namespace err {

/**
 * @brief Print an error message to stderr in
 * "ERROR `error_description`" format.
 * 
 * @param error_description The description of the error.
 */
void error(const std::string &error_description) {
    std::cerr << "ERROR: " << error_description << std::endl;
}

/**
 * @brief Print an error message to stderr in
 * "ERROR: bad message from [ip]:port, player: message_type\n" format.
 * 
 * @param message_type The type of the message.
 * @param player The player who sent the message.
 * @param addr The address of the sender.
 */
void message(const std::string &message_type,
             const std::string &player,
             const struct sockaddr_in &addr) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, INET_ADDRSTRLEN);
    uint16_t port = ntohs(addr.sin_port);

    std::cerr << "ERROR: bad message from [" << ip_str << "]:" << port
              << ", " << player << ": " << message_type << std::endl;
}

}

void game_end(const std::vector<std::pair<std::string, std::string>>& results) {
    std::cout << "Game end, scoring:";
    for (const auto& [player_id, result] : results) {
        std::cout << " " << player_id << " " << result;
    }
    std::cout << "." << std::endl;
}

} // namespace log
} // namespace tga