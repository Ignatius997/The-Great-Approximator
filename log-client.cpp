#include <iostream>

#include "log-client.h"

namespace tga {
namespace log {
namespace client {

// NOTE Needs testing.
// FIXME Make it thread-safe

/**
 * @brief Print a message when the client is connected to the server.
 * 
 * @param ip The IP address of the server.
 * @param port The port number of the server.
 */
void connected_to(const std::string& ip, uint16_t port) {
    std::cout << "Connected to [" << ip << "]:" << port << "." << std::endl;
}

/**
 * @brief Print a message when the client receives coefficients.
 * 
 * @param coeffs The coefficients received from the server.
 */
void received_coefficients(const std::vector<std::string>& coeffs) {
    std::cout << "Received coefficients";
    for (const auto& coeff : coeffs) {
        std::cout << " " << coeff;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when the client puts a value in a point.
 * 
 * @param value The value being put.
 * @param point The point where the value is being put.
 */
void putting_value(const std::string& value, int point) {
    std::cout << "Putting " << value << " in " << point << "." << std::endl;
}

/**
 * @brief Print a message when the client receives the state.
 * 
 * @param state The state received from the server.
 */
void received_state(const std::vector<std::string>& state) {
    std::cout << "Received state";
    for (const auto& s : state) {
        std::cout << " " << s;
    }
    std::cout << "." << std::endl;
}

} // namespace client
} // namespace log
} // namespace tga