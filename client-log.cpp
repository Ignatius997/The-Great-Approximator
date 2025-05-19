#include "client-log.h"
#include <iostream>

namespace tga {
namespace log {
namespace client {

// NOTE Needs testing.
// FIXME Make it thread-safe

void connected_to(const std::string& ip, uint16_t port) {
    std::cout << "Connected to [" << ip << "]:" << port << "." << std::endl;
}

void received_coefficients(const std::vector<std::string>& coeffs) {
    std::cout << "Received coefficients";
    for (const auto& coeff : coeffs) {
        std::cout << " " << coeff;
    }
    std::cout << "." << std::endl;
}

void putting_value(const std::string& value, int point) {
    std::cout << "Putting " << value << " in " << point << "." << std::endl;
}

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