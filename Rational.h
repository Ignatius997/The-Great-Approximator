#ifndef TGA_RAT_H
#define TGA_RAT_H

#include <string>
#include <cassert>

namespace tga {
namespace rat {

class Rational {
private:
    /**
     * (-)xxx.xxxxxxx, max 7 digits after dot.
     * Dot is optional, as well as digits after dot.
     */
    std::string num;

    static void remove_trailing_zeros(std::string &num) {
        auto dot_pos = num.find('.');
        if (dot_pos != std::string::npos) {
            // Usuń końcowe zera
            size_t last_nonzero = num.find_last_not_of('0');
            if (last_nonzero != std::string::npos && last_nonzero > dot_pos) {
                num.erase(last_nonzero + 1);
            }
            // Jeśli po kropce nie ma żadnych cyfr, usuń też kropkę
            if (num.back() == '.') {
                num.pop_back();
            }
        }
    }

    static void fit_length(std::string &num) {
        auto dot_pos = num.find('.');
        if (dot_pos != std::string::npos) {
            // Ensure that there are at most 7 digits after the dot
            size_t frac_len = num.size() - dot_pos - 1;
            assert(frac_len <= 7);
            if (frac_len > 7) {
                // Truncate to 7 digits after the dot
                num = num.substr(0, dot_pos + 1 + 7);
            }
        }
    }


public:
    Rational() : num("0") {} // Default constructor initializes to 0
    Rational(const double d) : num(std::to_string(d)) {
        fit_length(num);
        remove_trailing_zeros(num);
    }
    /**
     * @brief Constructs a Rational object from a string.
     * @note Constructor assumes that the input string is correctly formatted.
     */
    Rational(const std::string& value) : num(value) {}

    Rational operator+(const Rational& other) const {
        return Rational((double) *this + (double) other);
    }
    Rational& operator+=(const Rational& other) {
        *this = *this + other;
        return *this;
    }

    explicit operator std::string() const { return num; }
    explicit operator double() const { return std::stod(num); }
};

} // namespace rat
} // namespace tga

#endif // TGA_RAT_H