#ifndef TGA_UTILS_H
#define TGA_UTILS_H

namespace tga {
namespace utils {

/**
 * @brief This enum determines in which phase is currently
 * the communication between client and server.
 */
enum CommunicationPhase {
    // TODO Implement
};

// FIXME To powinno być raczej w netutils.h
/**
 * @brief This class is used to store information about the
 * received message.
 */
class ReceiveInfo {
public:
    std::string msg_type; // Message type, e.g., "HELLO", "COEFF", etc.
    int err = 0; // Error code, 0 if no error occurred.
};

} // namespace utils
} // namespace tga

#endif // TGA_UTILS_H