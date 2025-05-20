#ifndef TGA_GLOBAL_H
#define TGA_GLOBAL_H

namespace tga {
namespace global {

// Set debug value.
#ifdef NDEBUG
bool constexpr debug = false;
#else
bool constexpr debug = true;
#endif

// Set server and client values.
#ifdef SERVER
bool constexpr server = true;
bool constexpr client = false;
#else
bool constexpr server = false;
bool constexpr client = true;
#endif

} // namespace global
} // namespace tga

#endif // TGA_GLOBAL_H