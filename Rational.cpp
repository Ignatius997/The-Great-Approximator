#include <stdexcept>
#include <regex>

#include "Rational.h"

namespace tga {
namespace rat {

// FIXME Na razie unused, poza tym wymaga liczby po kropce
static std::regex rational_regex(R"(^-?\d+(\.\d{1,7})?$)");

// NOTE Należy pamiętać, że Rational dopuszcza liczby typu 2.,
// NOTE czyli z kropką, ale bez cyfr po kropce.

// ==== Operators ====

/**
 * @brief Adds two Rational numbers.
 * @param other The other Rational number to add.
 * @return A new Rational object representing the sum.
 */
Rational Rational::operator+(const Rational& other) const {
    return Rational(addition(num, other.num));
}

// ==== Auxiliary methods ====

std::string Rational::addition(const std::string& a, const std::string& b) {
    // TODO Implement addition of two rational numbers
    return a + " + " + b; // NOTE Placeholder implementation
}

} // namespace rat
} // namespace tga