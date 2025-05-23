#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

#include "utils.h"
#include "Message.h"

namespace tga {
namespace comm {

using tga::msg::Message;

namespace server {

} // namespace server

namespace client {

} // namespace client

// TODO Create some return type for send and/or receive and/or handle functions to determine what to do next.
void send_message(int fd, const Message &msg);
tga::utils::ReceiveInfo receive_message();
void handle_message(const tga::utils::ReceiveInfo &info);
void end();

} // comm
} // tga

#endif // TGA_COMMUNICATION_H