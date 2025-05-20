#ifndef TGA_COMMUNICATION_H
#define TGA_COMMUNICATION_H

namespace tga {
namespace communication {

// NOTE Implementations of those functions may vary depending on preprocessor `SERVER` and `CLIENT` macros.

// TODO Create some return type for send and/or receive and/or handle functions
// TODO to determine what to do next.
void send_message();
void receive_message();
void handle_message();

} // communication
} // tga

#endif // TGA_COMMUNICATION_H