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
    unsigned M();
}
namespace client {
    
}

unsigned port();

} // namespace args
} // namespace tga

#endif // TGA_ARGS_H