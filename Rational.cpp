#include <stdexcept>
#include <regex>

#include "Rational.h"

namespace tga {
namespace rat {

static std::regex rational_regex(R"(^-?\d+(\.\d{1,7})?$)");

// ==== Constructors ====

Rational::Rational() : num("0") {}

Rational::Rational(const std::string& value) {
    // Validate the input string using regex
    if (!std::regex_match(value, rational_regex)) {
        throw std::invalid_argument("Invalid rational number format: " + value);
    }
    num = value;
}

// ==== Operators ====
Rational Rational::operator+(const Rational& other) const {
    return Rational(addition(num, other.num));
}

Rational::operator std::string() const {
    return num;
}

// TODO Implement other operators if needed

// ==== Auxiliary methods ====
std::string Rational::addition(const std::string& a, const std::string& b) {
    // TODO Implement addition of two rational numbers
    return a + " + " + b; // Placeholder implementation
}

} // namespace rat
} // namespace tga