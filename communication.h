#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

#include "utils.h"

namespace tga {
namespace comm {

// NOTE Implementations of those functions may vary depending on preprocessor `SERVER` and `CLIENT` macros.

// TODO Create some return type for send and/or receive and/or handle functions
// TODO to determine what to do next.
void send_message();
tga::utils::ReceiveInfo receive_message();
void handle_message(const tga::utils::ReceiveInfo &info);
void end();

} // comm
} // tga

#endif // TGA_COMMUNICATION_H