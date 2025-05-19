#include <iostream>

#include "log.h"

namespace tga {
namespace log {

// NOTE Needs testing.
// FIXME Make it thread-safe.

void game_end(const std::vector<std::pair<std::string, std::string>>& results) {
    std::cout << "Game end, scoring:";
    for (const auto& [player_id, result] : results) {
        std::cout << " " << player_id << " " << result;
    }
    std::cout << "." << std::endl;
}

} // namespace log
} // namespace tga