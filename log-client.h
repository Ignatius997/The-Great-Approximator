#ifndef TGA_CLIENT_LOG_H
#define TGA_CLIENT_LOG_H

#include <string>
#include <vector>
#include <cstdint>

namespace tga {
namespace log {
namespace client {

void connected_to(const std::string& ip, uint16_t port);
void received_coefficients(const std::vector<std::string>& coeffs);
void putting_value(const std::string& value, int point);
void received_state(const std::vector<std::string>& state);

} // namespace client
} // namespace log
} // namespace tga

#endif // TGA_CLIENT_LOG_H