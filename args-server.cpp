#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "args-server.h"
#include "args.h"

namespace tga {
namespace args {
namespace server {

// NOTE Needs testing
// FIXME Make it thread-safe

/**
 * @brief Server command line arguments parser.
 * 
 * This class handles the parsing of command line arguments for the server.
 * It validates the input and sets default values where necessary.
 */
class ProgramArgs {
protected:
    unsigned port = 0;
    unsigned k = 100;
    unsigned n = 4;
    unsigned m = 131;
    std::string file;

public:
    void parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-p" && i + 1 < argc) {
                if (!tga::args::parse_uint(argv[++i], 0, 65535, port)) {
                    std::cerr << "Invalid port value\n";
                    std::exit(1);
                }
            } else if (arg == "-k" && i + 1 < argc) {
                if (!tga::args::parse_uint(argv[++i], 1, 10000, k)) {
                    std::cerr << "Invalid K value\n";
                    std::exit(1);
                }
            } else if (arg == "-n" && i + 1 < argc) {
                if (!tga::args::parse_uint(argv[++i], 1, 8, n)) {
                    std::cerr << "Invalid N value\n";
                    std::exit(1);
                }
            } else if (arg == "-m" && i + 1 < argc) {
                if (!tga::args::parse_uint(argv[++i], 1, 12341234, m)) {
                    std::cerr << "Invalid M value\n";
                    std::exit(1);
                }
            } else if (arg == "-f" && i + 1 < argc) {
                file = argv[++i];
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg << "\n";
                std::exit(1);
            }
        }
        if (file.empty()) {
            std::cerr << "Missing required parameter: -f file\n";
            std::exit(1);
        }
    }
};

void load(int argc, char** argv) {
    static ProgramArgs args;
    args.parse(argc, argv);
}

} // namespace server
} // namespace args
} // namespace tga