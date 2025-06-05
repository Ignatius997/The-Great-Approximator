#ifndef TGA_ARGS_H
#define TGA_ARGS_H

#include <string>
#include <optional>
#include <vector>

namespace tga {
namespace args {

void parse(int argc, char** argv);
void print();

namespace server {
    size_t M();
    size_t N();
    size_t K();
}

namespace client {
    std::string server();
    std::string player_id();
    int family();
    bool default_strategy();
}

unsigned port();

} // namespace args
} // namespace tga

#endif // TGA_ARGS_H