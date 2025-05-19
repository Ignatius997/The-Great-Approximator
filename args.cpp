#include <sstream>
#include <limits>

#include "args.h"

namespace tga {
namespace args {

bool parse_int(const std::string& str, int min, int max, int& out) {
    try {
        size_t idx;
        int val = std::stoi(str, &idx, 10);
        if (idx != str.size() || val < min || val > max) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_uint(const std::string& str, unsigned min, unsigned max, unsigned& out) {
    try {
        size_t idx;
        unsigned long val = std::stoul(str, &idx, 10);
        if (idx != str.size() || val < min || val > max) return false;
        out = static_cast<unsigned>(val);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

} // namespace args
} // namespace tga