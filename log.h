#ifndef TGA_LOG_H
#define TGA_LOG_H

#include <string>
#include <vector>
#include <utility>

namespace tga{
namespace log{

void game_end(const std::vector<std::pair<std::string, std::string>>& results);

} // namespace log
} // namespace tga

#endif // TGA_LOG_H