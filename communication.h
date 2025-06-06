#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

#include <variant>
#include <optional>

#include "netutils.h"
#include "Message.h"
#include "MessageCombinators.h"

namespace tga {
namespace comm {

using tga::msg::Message;
using tga::net::ReceiveInfo;
using tga::msg::MsgPtr;

namespace server {
    size_t get_messages_to_receive();
    size_t poll_structure_size();

    void clear_revents();
    int poll_events();
    void handle_poll_event(const size_t idx);

    void update_timeouts();
    void handle_timeouts();
    void new_clients(const int family);
    void send_scores();

    bool connection_exists(const size_t idx);
    bool new_ipv4_clients();
    bool new_ipv6_clients();
    bool should_send_scores();
}

namespace client {
    void clear_revents();
    int  poll_events();
    void prepare_to_send(MsgPtr msg);
    std::optional<int> handle_poll_event_from_server();
    void handle_poll_event_from_stdin();
}

void setup();

} // comm
} // tga

#endif // TGA_COMMUNICATION_H