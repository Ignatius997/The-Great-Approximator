#include <string>
#include <fstream>
#include <iostream>

#include "io.h"

namespace tga {
namespace io {

namespace {

    // FIXME Make `current_line` thread-safe
std::string coeffs_file; // File with coefficients
size_t current_line = 0; // Line to be read next.

} // anonymous namespace

// FIXME Należy zadbać o to, żeby nie wykonywano tego jednocześnie przez wiele wątków/procesów.
// NOTE Not tested
/**
 * @brief Reads coefficients from a file named "coeffs.txt".
 * 
 * This function attempts to open file with coefficients and read from it one line
 * storing it into a string. If the file cannot be opened, it throws a runtime error.
 * 
 * @return A string containing coefficients.
 * @throws std::runtime_error if the file cannot be opened.
 */
std::string read_coeffs() {
    std::ifstream file(coeffs_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open" + coeffs_file);
    }

    std::string line;
    size_t line_number = 0;

    while (std::getline(file, line)) {
        if (line_number == current_line) {
            // We assume that the line is in the format "COEFF <coefficients>\r\n".
            line.erase(line.find_last_not_of("\r") + 1); // Delete '\r' at the end
            line = line.substr(6); // Ignore "COEFF "
            break;
        }
        ++line_number;
    }

    if (line_number <= current_line) {
        throw std::runtime_error("Line number out of range in " + coeffs_file);
    }

    ++current_line; // Increment for the next read

    file.close();
    return line;
}

} // namespace io
} // namespace tga