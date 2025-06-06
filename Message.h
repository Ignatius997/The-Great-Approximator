#ifndef TGA_MSG_H
#define TGA_MSG_H

#include <string>

namespace tga {
namespace msg {

/**
 * @brief Abstract class representing a message sent between server and a client.
 * @note Every class derived from Message must implement a deserialize function (see MessageCombinators.h).
 */
class Message {
public:
    /**
     * @brief Returns the type or name of the message.
     * 
     * This function should be overridden by derived classes to provide
     * a unique identifier for the message type.
     * 
     * @return A string representing the message type.
     */
    virtual std::string message_type() const = 0;

    /**
     * @brief Returns the content of the message.
     * 
     * This function should be overridden by derived classes to provide
     * the specific content of the message.
     * 
     * @return A string representing the message content.
     */
    virtual std::string message_content() const = 0;

    /**
     * @brief Serializes the message into a string format.
     * 
     * Combines the message type and content into a single string,
     * separated by a space, and appends a carriage return and newline.
     * 
     * @return A serialized string representation of the message.
     */
    std::string serialize() const {
        return message_type() + " " + message_content() + "\r\n";
    }

    virtual ~Message() = default; // Virtual destructor for proper inheritance
};

} // namespace msg
} // namespace tga

#endif // TGA_MSG_H