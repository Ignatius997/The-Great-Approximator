

#include "communication.h"
#include "utils.h"

namespace tga {
namespace comm {
    
using tga::msg::Message;

void send_message(int fd, const Message &msg) {
    std::string serialized_message = msg.serialize();
    // write(fd, serialized_message.c_str(), serialized_message.size());
}

tga::utils::ReceiveInfo receive_message() {
    return tga::utils::ReceiveInfo{};
    // TODO Implement this function
}

void handle_message(const tga::utils::ReceiveInfo &info) {
    (void) info;
    // TODO Implement this function
}

void end() {
    // send SCORING messages to clients
    // close connections
    // exit
}

} // comm
} // tga