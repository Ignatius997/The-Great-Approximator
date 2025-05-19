#include "server_log.h"
#include <iostream>

// NOTE Needs testing.
// FIXME Make it thread-safe

namespace tga {
namespace log {
namespace server {

void new_client(const std::string& ip, uint16_t port) {
    std::cout << "New client [" << ip << "]:" << port << "." << std::endl;
}

void client_known(const std::string& ip, uint16_t port, const std::string& player_id) {
    std::cout << ip << ":" << port << " is now known as " << player_id << "." << std::endl;
}

void get_coefficients(const std::string& player_id, const std::vector<std::string>& coeffs) {
    std::cout << player_id << " get coefficients";
    for (const auto& coeff : coeffs) {
        std::cout << " " << coeff;
    }
    std::cout << "." << std::endl;
}

void put_value(const std::string& player_id, const std::string& value, int point, const std::vector<std::string>& state) {
    std::cout << player_id << " puts " << value << " in " << point << ", current state";
    for (const auto& s : state) {
        std::cout << " " << s;
    }
    std::cout << "." << std::endl;
}

void send_state(const std::vector<std::string>& state, const std::string& player_id) {
    std::cout << "Sending state";
    for (const auto& s : state) {
        std::cout << " " << s;
    }
    std::cout << " to " << player_id << "." << std::endl;
}

} // namespace server
} // namespace log
} // namespace tga