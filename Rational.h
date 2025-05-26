#ifndef TGA_RAT_H
#define TGA_RAT_H

#include <string>

namespace tga {
namespace rat {

class Rational {
private:
    /**
     * (-)xxx.xxxxxxx, max 7 digits after dot.
     * Dot is optional, as well as digits after dot.
     */
    std::string num;

    static std::string addition(const std::string& a, const std::string& b);

public:
    Rational() : num("0") {} // Default constructor initializes to 0

    /**
     * @brief Constructs a Rational object from a string.
     * @note Constructor assumes that the input string is correctly formatted.
     */
    Rational(const std::string& value) : num(value) {}

    Rational operator+(const Rational& other) const;

    bool operator==(const Rational& other) const;
    bool operator!=(const Rational& other) const;

    explicit operator std::string() const { return num; }
    explicit operator double() const { return std::stod(num); }
};

} // namespace rat
} // namespace tga

#endif // TGA_RAT_H