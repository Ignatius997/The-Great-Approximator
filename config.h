#ifndef TGA_CONFIG_H
#define TGA_CONFIG_H

namespace tga {
namespace config {

// Set debug value.
#if defined(NDEBUG)
constexpr bool debug = false;
#else
constexpr bool debug = true;
#endif

// Set server value.
#if defined(TGA_SERVER) && defined(TGA_CLIENT)
    #error "Both TGA_SERVER and TGA_CLIENT preprocessor macros are defined. Only one should be defined."
#elif !defined(TGA_SERVER) && !defined(TGA_CLIENT)
    #error "Neither TGA_SERVER nor TGA_CLIENT preprocessor macro is defined. One must be defined."
#endif

#if defined(TGA_SERVER) && !defined(TGA_CLIENT)
constexpr bool server = true;
#else
constexpr bool server = false;
#endif

} // namespace config
} // namespace tga

#endif // TGA_CONFIG_H