#ifndef TGA_ERR_H
#define TGA_ERR_H

#include <string>
#include <arpa/inet.h>

namespace tga {
namespace err {

void error(const std::string &error_description);
void message(const std::string &message_type, const std::string &player, const struct sockaddr_in &addr);

} // namespace err
} // namespace tga

#endif // TGA_ERR_H