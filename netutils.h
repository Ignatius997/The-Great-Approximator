#ifndef TGA_NUTILS_H
#define TGA_NUTILS_H

#include <string>
#include <cstdint>
#include <netinet/in.h>

namespace tga {
namespace net {

struct sockaddr_in get_server_address(const std::string &host, unsigned port);
void setup();

} // namespace net
} // namespace tga

#endif // TGA_NUTIL_H