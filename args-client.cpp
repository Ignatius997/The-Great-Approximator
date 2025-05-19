#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "args-client.h"
#include "args.h"

namespace tga {
namespace args {
namespace client {

// NOTE Needs testing
// FIXME Make it thread-safe

class ProgramArgs {
protected:
    std::string player_id;
    std::string server;
    unsigned port = 0;
    bool force_ipv4 = false;
    bool force_ipv6 = false;
    bool strategy_a = false;

public:

    void parse(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-u" && i + 1 < argc) {
                player_id = argv[++i];
            } else if (arg == "-s" && i + 1 < argc) {
                server = argv[++i];
            } else if (arg == "-p" && i + 1 < argc) {
                if (!tga::args::parse_uint(argv[++i], 1, 65535, port)) {
                    std::cerr << "Invalid port value\n";
                    std::exit(1);
                }
            } else if (arg == "-4") {
                force_ipv4 = true;
            } else if (arg == "-6") {
                force_ipv6 = true;
            } else if (arg == "-a") {
                strategy_a = true;
            } else {
                std::cerr << "Unknown or incomplete argument: " << arg << "\n";
                std::exit(1);
            }
        }
        if (player_id.empty() || server.empty() || port == 0) {
            std::cerr << "Missing required parameters\n";
            std::exit(1);
        }
        if (force_ipv4 && force_ipv6) {
            std::cerr << "Cannot specify both -4 and -6\n";
            std::exit(1);
        }
    }
};

void load(int argc, char** argv) {
    static ProgramArgs args;
    args.parse(argc, argv);
}

} // namespace client
} // namespace args
} // namespace tga