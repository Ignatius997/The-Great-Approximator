#ifndef TGA_RAT_H
#define TGA_RAT_H

#include <string>

namespace tga {
namespace rat {

class Rational {
private:
    std::string num; // (-)xxx.xxxxxxx, max 7 digits after dot. Dot is optional.

    static std::string addition(const std::string& a, const std::string& b);

public:
    Rational();
    Rational(const std::string& value);

    Rational operator+(const Rational& other) const;
    Rational operator-(const Rational& other) const;
    Rational operator*(const Rational& other) const;
    Rational operator/(const Rational& other) const;

    bool operator==(const Rational& other) const;
    bool operator!=(const Rational& other) const;

    explicit operator std::string() const;
};

} // namespace rat
} // namespace tga

#endif // TGA_RAT_H