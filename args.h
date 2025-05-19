#ifndef TGA_ARGS_H
#define TGA_ARGS_H

#include <string>
#include <optional>
#include <vector>

namespace tga {
namespace args {

/**
 * @brief Helper: check if string is a valid integer in range [min, max]
 * @param str String to check
 * @param min Minimum value
 * @param max Maximum value
 * @param out Output variable to store the parsed value
 */
bool parse_int(const std::string& str, int min, int max, int& out);

/**
 * @brief Helper: check if string is a valid unsigned integer in range [min, max]
 * @param str String to check
 * @param min Minimum value
 * @param max Maximum value
 * @param out Output variable to store the parsed value
 */
bool parse_uint(const std::string& str, unsigned min, unsigned max, unsigned& out);

/**
 * @brief Helper: split string by delimiter
 * @param str String to split
 * @param delim Delimiter character
 * @return Vector of strings
 */
std::vector<std::string> split(const std::string& str, char delim);

} // namespace args
} // namespace tga

#endif // TGA_ARGS_H