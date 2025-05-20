#include <iostream>

#include "log-server.h"

// NOTE Needs testing.
// FIXME Make it thread-safe

namespace tga {
namespace log {
namespace server {

/**
 * @brief Print a message when a new client connects.
 * 
 * @param ip The IP address of the client.
 * @param port The port number of the client.
 */
void new_client(const std::string& ip, uint16_t port) {
    std::cout << "New client [" << ip << "]:" << port << "." << std::endl;
}

/**
 * @brief Print a message when a client is known.
 * 
 * @param ip The IP address of the client.
 * @param port The port number of the client.
 * @param player_id The ID of the player.
 */
void client_known(const std::string& ip, uint16_t port, const std::string& player_id) {
    std::cout << ip << ":" << port << " is now known as " << player_id << "." << std::endl;
}

/**
 * @brief Print a message when coefficients are requested.
 * 
 * @param player_id The ID of the player requesting coefficients.
 * @param coeffs The coefficients being requested.
 */
void get_coefficients(const std::string& player_id, const std::vector<std::string>& coeffs) {
    std::cout << player_id << " get coefficients";
    for (const auto& coeff : coeffs) {
        std::cout << " " << coeff;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when a value is put in a point.
 * 
 * @param player_id The ID of the player putting the value.
 * @param value The value being put.
 * @param point The point where the value is being put.
 * @param state The current state of the game.
 */
void put_value(const std::string& player_id, const std::string& value, int point, const std::vector<std::string>& state) {
    std::cout << player_id << " puts " << value << " in " << point << ", current state";
    for (const auto& s : state) {
        std::cout << " " << s;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when the state is sent to a player.
 * 
 * @param state The state being sent.
 * @param player_id The ID of the player receiving the state.
 */
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