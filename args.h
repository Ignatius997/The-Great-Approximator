#ifndef TGA_ARGS_H
#define TGA_ARGS_H

#include <string>
#include <optional>
#include <vector>

namespace tga {
namespace args {

void parse(int argc, char** argv);
void print();

#ifdef SERVER
namespace server {

unsigned M();

} // namespace server
#endif // SERVER

#ifdef CLIENT
namespace client {
    
}
#endif // CLIENT

unsigned port();

} // namespace args
} // namespace tga

#endif // TGA_ARGS_H