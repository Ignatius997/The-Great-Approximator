#include <stdexcept>
#include <regex>

#include "Rational.h"

namespace tga {
namespace rat {

static std::regex rational_regex(R"(^-?\d+(\.\d{1,7})?$)");

// ==== Constructors ====

/**
 * @brief Default constructor for Rational class.
 * Initializes the rational number to 0.
 */
Rational::Rational() : num("0") {}

/**
 * @brief Constructs a Rational object from a string.
 */
Rational::Rational(const std::string& value) {
    // Validate the input string using regex
    if (!std::regex_match(value, rational_regex)) {
        throw std::invalid_argument("Invalid rational number format: " + value);
    }
    num = value;
}

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