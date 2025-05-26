#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

#include <variant>

#include "utils.h"
#include "Message.h"

namespace tga {
namespace comm {

using tga::msg::Message;

namespace client {
    int get_sockfd();
}

void setup();
void send_message(int fd, const Message &msg);
tga::utils::ReceiveInfo receive_message();
void handle_message(const tga::utils::ReceiveInfo &info);
void end();

} // comm
} // tga

#endif // TGA_COMMUNICATION_H