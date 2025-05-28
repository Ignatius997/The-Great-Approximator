#include <string>
#include <fstream>
#include <iostream>
#include <cassert>
#include <vector>
#include <utility>

#include "io.h"
#include "config.h"
#include "netutils.h"

namespace tga {
namespace io {

namespace log {

namespace err {

/**
 * @brief Print an error message to stderr in
 * "ERROR `error_description`" format.
 * 
 * @param error_description The description of the error.
 */
void error(const std::string &error_description) {
    std::cerr << "ERROR: " << error_description << std::endl;
    if (tga::config::debug) exit(1);
}

/**
 * @brief Print an error message to stderr in
 * "ERROR: bad message from [ip]:port, player: message_type\n" format.
 * 
 * @param message_type The type of the message.
 * @param player The player who sent the message.
 * @param addr The address of the sender.
 */
void message(const std::string &message,
             const std::string &player,
             const SockAddrVariant &addr) {
    auto ip = tga::net::get_ip(addr);
    auto port = tga::net::get_port(addr);
    std::cerr << "ERROR: bad message from [" << ip << "]:" << port
              << ", " << player << ": " << message << std::endl;
}

} // namespace err

namespace info {

/**
 * @brief Print game end message with scoring.
 * 
 * @param results Vector of pairs containing player ID and result.
 */
void game_end(const std::vector<std::pair<std::string, std::string>>& results) {
    std::cout << "Game end, scoring:";
    for (const auto& [player_id, result] : results) {
        std::cout << " " << player_id << " " << result;
    }
    std::cout << "." << std::endl;
}

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

namespace client {

// NOTE Needs testing.
// FIXME Make it thread-safe

/**
 * @brief Print a message when the client is connected to the server.
 * 
 * @param ip The IP address of the server.
 * @param port The port number of the server (host byte order).
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

} // namespace info
} // namespace log

namespace file {

namespace {
    std::string coeffs_file; // File with coefficients
    size_t current_line = 0; // Line to be read next.
}


// NOTE Not tested
/**
 * @brief Reads coefficients from a file named "coeffs.txt".
 * 
 * This function attempts to open file with coefficients and read from it one line
 * storing it into a string. If the file cannot be opened, it logs an error and exits the program.
 * 
 * @return A string containing coefficients in "$c_0 $c_2 ... $c_N" format.
 */
std::string read_coeffs() {
    std::ifstream file(coeffs_file);
    if (!file.is_open()) {
        tga::io::log::err::error("Failed to open" + coeffs_file);
        exit(1);
    }

    std::string line;
    size_t line_number = 0;

    while (std::getline(file, line)) {
        if (line_number == current_line) {
            // We assume that the line is in the format "COEFF <coefficients>\r\n".
            line.erase(line.find_last_not_of("\r") + 1); // Delete '\r' at the end.
            line = line.substr(6); // Ignore "COEFF "
            break;
        }
        ++line_number;
    }

    assert(line_number <= current_line); // Should not occur.
    ++current_line; // Increment for the next read.

    file.close();
    return line;
}

}

} // namespace io
} // namespace tga