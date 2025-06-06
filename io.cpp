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

namespace {

/**
 * @brief Convert an IP address to a more convenient format.
 */
std::string convenient_ip(const std::string &ip) {
    // If the IP is a loopback address, return "localhost".
    if (ip == "::1") return "localhost";
    return ip;
}

} // anonymous namespace

namespace err {

/**
 * @brief Print an error message to stderr in
 * "ERROR `error_description`" format.
 * 
 * @param error_description The description of the error.
 */
void error(const std::string &error_description) {
    std::cerr << "ERROR: " << error_description << std::endl;
}

namespace server {

/**
 * @brief Print an error message to stderr in
 * "ERROR: bad message from [ip]:port, player: <message>\n" format.
 * 
 * @param message The message.
 * @param player The player who sent the message.
 * @param addr The address of the sender.
 */
void message(const std::string &message,
             const std::string &player,
             const SockAddrVariant &addr) {
    auto ip = tga::net::get_ip(addr);
    auto port = tga::net::get_port(addr);
    std::cerr << "ERROR: bad message from [" << convenient_ip(ip) << "]:" << port
              << ", " << player << ": " << message << std::endl;
}

void input(const std::string &input) {
    std::cerr << "ERROR: invalid input line: " << input << std::endl;
}

} // namespace server

namespace client {

/**
 * @brief Print an error message to stderr in
 * "ERROR: bad message from server: <message>\n" format.
 * 
 * @param message The message.
 */
void message(const std::string &message) {
    std::cerr << "ERROR: bad message from server: " << message << std::endl;
}

}

} // namespace err

namespace info {

/**
 * @brief Print a custom message to stdout.
 * @param message The message to be printed.
 */
void custom(const std::string &message) {
    std::cout << message << std::endl;
}

/**
 * @brief Print a log when a message is sent to a player/server.
 * 
 * @param message The message sent.
 * @param player_id The ID of the player who received the message.
 *                  Redundant if function called by a client 
 * @param addr The address of the recipient.
 */
void sent(const std::string& message, const std::string& player_id) {
    size_t start = 0;
    while (start < message.size()) {
        size_t crlf = message.find("\r\n", start);
        std::string line = (crlf != std::string::npos)
            ? message.substr(start, crlf - start)
            : message.substr(start);

        // Extract message type (first word)
        size_t space = line.find(' ');
        std::string type = (space != std::string::npos) ? line.substr(0, space) : line;
        std::string rest = (space != std::string::npos) ? line.substr(space + 1) : "";

        // Handle different message types
        if (type == "COEFF") {
            std::cout << player_id << " get coefficients " << rest << "." << std::endl;
        } else if (type == "PUT") {
            size_t point_end = rest.find(' ');
            if (point_end != std::string::npos) {
                std::string point = rest.substr(0, point_end);
                std::string value = rest.substr(point_end + 1);
                std::cout << "Putting " << value << " in " << point << "." << std::endl;
            }
        } else if (type == "STATE") {
            std::cout << "Sending state " << rest << " to " << player_id << "." << std::endl;
        } else if (type == "SCORING") {
            std::cout << "Game end, scoring: " << rest << "." << std::endl;
        }

        if (crlf == std::string::npos) break;
        start = crlf + 2; // Move past "\r\n"
    }
}

namespace server {

/**
 * @brief Print a message when a new client connects.
 * 
 * @param ip The IP address of the client.
 * @param port The port number of the client.
 */
void new_client(const std::string& ip, uint16_t port) {
    std::cout << "New client [" << convenient_ip(ip) << "]:" << port << "." << std::endl;
}

/**
 * @brief Print a message when a client is known.
 * 
 * @param ip The IP address of the client.
 * @param port The port number of the client.
 * @param player_id The ID of the player.
 */
void hello(const std::string& ip, uint16_t port, const std::string& player_id) {
    std::cout << "" << convenient_ip(ip) << ":" << port << " is now known as " << player_id << "." << std::endl;
}

/**
 * @brief Print a message when coefficients are requested.
 * 
 * @param player_id The ID of the player requesting coefficients.
 * @param coeffs The coefficients being requested.
 */
void coeff(const std::string& player_id, const std::vector<std::string>& coeffs) {
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
void put(const std::string& player_id, int point, const std::string& value, const std::vector<std::string>& state) {
    std::cout << player_id << " puts " << value << " in " << point << ", current state";
    for (const auto& s : state) {
        std::cout << " " << s;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when a client disconnects.
 * 
 * @param ip The IP address of the client.
 * @param port The port number of the client.
 * @param player_id The ID of the player who disconnected.
 */
void client_disconnected(const std::string& ip, uint16_t port, const std::string& player_id) {
    std::cout << "Player " << player_id << " (" << convenient_ip(ip) << ":" << port << ")" 
              << " disconnected." << std::endl;
}

} // namespace server

namespace client {

/**
 * @brief Print a message when the client is connected to the server.
 * 
 * @param ip The IP address of the server.
 * @param port The port number of the server (host byte order).
 */
void connected_to(const std::string& ip, uint16_t port) {
    std::cout << "Connected to [" << convenient_ip(ip) << "]:" << port << "." << std::endl;
}

/**
 * @brief Print a message when the client receives COEFF message.
 * 
 * @param coeffs The coefficients received from the server.
 */
void coeff(const std::vector<std::string>& coeffs) {
    std::cout << "Received coefficients";
    for (const auto& coeff : coeffs) {
        std::cout << " " << coeff;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when the client receives the STATE message.
 * 
 * @param state The STATE coefficients received from the server.
 */
void state(const std::vector<std::string>& coeffs) {
    std::cout << "Received state";
    for (const auto& s : coeffs) {
        std::cout << " " << s;
    }
    std::cout << "." << std::endl;
}

void scoring(const std::vector<std::string>& scores) {
    std::cout << "Game end, scoring: ";
    for (const auto& score : scores) {
        std::cout << " " << score;
    }
    std::cout << "." << std::endl;
}

/**
 * @brief Print a message when the client receives a message from the server.
 * @param message The message received from the server.
 */
void received(const std::string &message) {
    size_t pos = message.find("\r\n");
    std::string cut_msg = (pos != std::string::npos) ? message.substr(0, pos) : message;
    std::cout << "Received message from server: " << cut_msg << std::endl;
}

void sent(const std::string &message) {
    size_t pos = message.find("\r\n");
    std::string cut_msg = (pos != std::string::npos) ? message.substr(0, pos) : message;
    std::cout << "Sent message to server: " << cut_msg << std::endl;
}

} // namespace client

} // namespace info
} // namespace log

namespace file {

namespace {
    std::string coeffs_file; // File with coefficients
    size_t current_line = 0; // Line to be read next.
}

/**
 * @brief Sets the coefficients file to be read.
 * This function sets the file with coefficients to be read by the `read_coeffs` function.
 * 
 * @param file The path to the coefficients file.
 */
void set_coeffs_file(const std::string& file) {
    coeffs_file = file;
}

/**
 * @brief Reads coefficients from a file with coefficients.
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

} // namespace file

} // namespace io
} // namespace tga