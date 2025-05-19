#ifndef TGA_SERVER_LOG_H
#define TGA_SERVER_LOG_H

#include <string>
#include <vector>
#include <cstdint>

namespace tga {
namespace log {
namespace server {

void new_client(const std::string& ip, uint16_t port);
void client_known(const std::string& ip, uint16_t port, const std::string& player_id);
void get_coefficients(const std::string& player_id, const std::vector<std::string>& coeffs);
void put_value(const std::string& player_id, const std::string& value, int point, const std::vector<std::string>& state);
void send_state(const std::vector<std::string>& state, const std::string& player_id);

} // namespace server
} // namespace log
} // namespace tga

#endif // TGA_SERVER_LOG_H