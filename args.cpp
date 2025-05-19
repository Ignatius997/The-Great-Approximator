#include <sstream>
#include <limits>
#include <vector>
#include <cstdlib>
#include <string>
#include <iostream>
#include <memory>

#include "args.h"
#include "err.h"

namespace tga {
namespace args {

namespace {

// ==== Auxiliary functions ====

static bool parse_uint(const std::string& str, unsigned min, unsigned max, unsigned& out) {
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

class ProgramArgs {
public:
    virtual void parse(int argc, char** argv) = 0;
    virtual void print() const = 0;
    virtual ~ProgramArgs() = default;
};

#ifdef SERVER
class ServerArgs : public ProgramArgs {
public:
    unsigned port = 0;
    unsigned k = 100;
    unsigned n = 4;
    unsigned m = 131;
    std::string file;

    void parse(int argc, char** argv) override final {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-p" && i + 1 < argc) {
                if (!parse_uint(argv[++i], 0, 65535, port)) {
                    tga::err::error("Invalid port value");
                    std::exit(1);
                }
            } else if (arg == "-k" && i + 1 < argc) {
                if (!parse_uint(argv[++i], 1, 10000, k)) {
                    tga::err::error("Invalid K value");
                    std::exit(1);
                }
            } else if (arg == "-n" && i + 1 < argc) {
                if (!parse_uint(argv[++i], 1, 8, n)) {
                    tga::err::error("Invalid N value");
                    std::exit(1);
                }
            } else if (arg == "-m" && i + 1 < argc) {
                if (!parse_uint(argv[++i], 1, 12341234, m)) {
                    tga::err::error("Invalid M value");
                    std::exit(1);
                }
            } else if (arg == "-f" && i + 1 < argc) {
                file = argv[++i];
            } else {
                tga::err::error("Unknown or incomplete argument: " + arg);
                std::exit(1);
            }
        }
        if (file.empty()) {
            tga::err::error("Missing required parameter: -f file");
            std::exit(1);
        }
    }

    void print() const override final {
        std::cout << "ServerArgs:\n";
        std::cout << "  port = " << port << "\n";
        std::cout << "  k = " << k << "\n";
        std::cout << "  n = " << n << "\n";
        std::cout << "  m = " << m << "\n";
        std::cout << "  file = " << file << "\n";
    }
};
#endif // SERVER

#ifdef CLIENT
class ClientArgs : public ProgramArgs {
public:
    std::string player_id;
    std::string server;
    unsigned port = 0;
    bool force_ipv4 = false;
    bool force_ipv6 = false;
    bool strategy_a = false;

    void parse(int argc, char** argv) override final {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-u" && i + 1 < argc) {
                player_id = argv[++i];
            } else if (arg == "-s" && i + 1 < argc) {
                server = argv[++i];
            } else if (arg == "-p" && i + 1 < argc) {
                if (!parse_uint(argv[++i], 1, 65535, port)) {
                    tga::err::error("Invalid port value");
                    std::exit(1);
                }
            } else if (arg == "-4") {
                force_ipv4 = true;
            } else if (arg == "-6") {
                force_ipv6 = true;
            } else if (arg == "-a") {
                strategy_a = true;
            } else {
                tga::err::error("Unknown or incomplete argument: " + arg);
                std::exit(1);
            }
        }
        if (player_id.empty() || server.empty() || port == 0) {
            tga::err::error("Missing required parameters: -u <player_id>, -s <server>, -p <port>");
            std::exit(1);
        }
        if (force_ipv4 && force_ipv6) {
            tga::err::error("Cannot specify both -4 and -6");
            std::exit(1);
        }
    }

    void print() const override final {
        std::cout << "ClientArgs:\n";
        std::cout << "  player_id = " << player_id << "\n";
        std::cout << "  server = " << server << "\n";
        std::cout << "  port = " << port << "\n";
        std::cout << "  force_ipv4 = " << (force_ipv4 ? "true" : "false") << "\n";
        std::cout << "  force_ipv6 = " << (force_ipv6 ? "true" : "false") << "\n";
        std::cout << "  strategy_a = " << (strategy_a ? "true" : "false") << "\n";
    }
};
#endif // CLIENT

static std::unique_ptr<ProgramArgs> make_args() {
#ifdef SERVER
    return std::make_unique<ServerArgs>();
#endif
#ifdef CLIENT
    return std::make_unique<ClientArgs>();
#endif
    return nullptr;
}

static std::unique_ptr<ProgramArgs> args;

} // anonymous namespace

/**
 * @brief Parses command line arguments.
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * Should be called before any other function in this namespace.
 */
void parse(int argc, char** argv) {
    args = make_args();
    args->parse(argc, argv);
}

/**
 * @brief Prints the parsed arguments.
 * Should be called after parse() to display the arguments.
 */
void print() {
    if (args) {
        args->print();
    } else {
        tga::err::error("No arguments parsed yet.");
    }
}

} // namespace args
} // namespace tga