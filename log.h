#ifndef TGA_LOG_H
#define TGA_LOG_H

#include <string>
#include <vector>
#include <utility>
#include <netinet/in.h>

namespace tga {
namespace log {
namespace err {

void error(const std::string &error_description);
void message(const std::string &message_type,
             const std::string &player,
             const struct sockaddr_in &addr);

} // namespace err

void game_end(const std::vector<std::pair<std::string, std::string>>& results);

} // namespace log
} // namespace tga

#endif // TGA_LOG_H