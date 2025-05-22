

#include "communication.h"
#include "utils.h"

namespace tga {
namespace comm {
    
void send_message() {

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