#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

#include <variant>

#include "netutils.h"
#include "Message.h"

namespace tga {
namespace comm {

using tga::msg::Message;
using tga::net::ReceiveInfo;

namespace server {
    size_t messages_to_receive();
    size_t poll_structure_size();

    void clear_revents();
    void update_timeouts();
    void handle_timeouts();
    void handle_poll_event(const size_t idx, ReceiveInfo &rinfo);
    void new_clients(const int family);
    void end();
    int poll_events();

    bool connection_exists(const size_t idx);
    bool new_ipv4_clients();
    bool new_ipv6_clients();
}

namespace client {
    int get_sockfd();
}

void setup();
void send_message(int fd, const Message &msg);

} // comm
} // tga

#endif // TGA_COMMUNICATION_H