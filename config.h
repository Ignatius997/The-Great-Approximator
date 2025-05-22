#ifndef TGA_CONFIG_H
#define TGA_CONFIG_H

namespace tga {
namespace config {

// Set debug value.
#if defined(NDEBUG)
bool constexpr debug = false;
#else
bool constexpr debug = true;
#endif

// Set server and client values.
#if defined(TGA_SERVER) && !defined(TGA_CLIENT)
bool constexpr server = true;
bool constexpr client = false;
#elif defined(TGA_CLIENT) && !defined(TGA_SERVER)
bool constexpr server = false;
bool constexpr client = true;
#else
    #error "Exactly one of TGA_SERVER or TGA_CLIENT preprocessor macro must be defined."
#endif


} // namespace config
} // namespace tga

#endif // TGA_CONFIG_H