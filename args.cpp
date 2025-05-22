#include <sstream>
#include <limits>
#include <vector>
#include <cstdlib>
#include <string>
#include <iostream>
#include <memory>
#include <cassert>
#include <variant>

#include "args.h"
#include "netutils.h"
#include "io.h"
#include "config.h"

namespace tga {
namespace args {

namespace {

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
protected:
    unsigned port = 0;

public:
    virtual void parse(int argc, char** argv) = 0;
    virtual void print() const = 0;
    virtual ~ProgramArgs() = default;
    unsigned getPort() { return port; }
};

class ServerArgs : public ProgramArgs {
protected:
    // unsigned port = 0;
    unsigned K = 100;
    unsigned N = 4;
    unsigned M = 131;
    std::string file;

public:
    void parse(int argc, char** argv) override final {
        bool port_set = false, k_set = false, n_set = false, m_set = false, file_set = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-p" && i + 1 < argc) {
                if (port_set) tga::io::log::err::error("Parameter -p specified multiple times");
                port_set = true;
                if (!parse_uint(argv[++i], 0, 65535, port)) {
                    tga::io::log::err::error("Invalid port value");
                    std::exit(1);
                }
            } else if (arg == "-k" && i + 1 < argc) {
                if (k_set) tga::io::log::err::error("Parameter -k specified multiple times");
                k_set = true;
                if (!parse_uint(argv[++i], 1, 10000, K)) {
                    tga::io::log::err::error("Invalid K value");
                    std::exit(1);
                }
            } else if (arg == "-n" && i + 1 < argc) {
                if (n_set) tga::io::log::err::error("Parameter -n specified multiple times");
                n_set = true;
                if (!parse_uint(argv[++i], 1, 8, N)) {
                    tga::io::log::err::error("Invalid N value");
                    std::exit(1);
                }
            } else if (arg == "-m" && i + 1 < argc) {
                if (m_set) tga::io::log::err::error("Parameter -m specified multiple times");
                m_set = true;
                if (!parse_uint(argv[++i], 1, 12341234, M)) {
                    tga::io::log::err::error("Invalid M value");
                    std::exit(1);
                }
            } else if (arg == "-f" && i + 1 < argc) {
                if (file_set) tga::io::log::err::error("Parameter -f specified multiple times");
                file_set = true;
                file = argv[++i];
            } else {
                tga::io::log::err::error("Unknown or incomplete argument: " + arg);
                std::exit(1);
            }
        }
        if (!file_set || file.empty()) {
            tga::io::log::err::error("Missing required parameter: -f file");
            std::exit(1);
        }
    }

    unsigned getM() { return M; }

    void print() const override final {
        std::cout << "ServerArgs:\n";
        std::cout << "  port = " << port << "\n";
        std::cout << "  k = " << K << "\n";
        std::cout << "  n = " << N << "\n";
        std::cout << "  m = " << M << "\n";
        std::cout << "  file = " << file << "\n";
    }
};

class ClientArgs : public ProgramArgs {
protected:
    std::string player_id;
    std::string server;
    bool force_ipv4 = false;
    bool force_ipv6 = false;
    bool strategy_a = false;

public:
    void parse(int argc, char** argv) override final {
        bool player_id_set = false, server_set = false, port_set = false;
        int ipv4_count = 0, ipv6_count = 0, strategy_a_count = 0;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-u" && i + 1 < argc) {
                if (player_id_set) tga::io::log::err::error("Parameter -u specified multiple times");
                player_id_set = true;
                player_id = argv[++i];
                // Walidacja player_id: tylko litery i cyfry
                for (char c : player_id) {
                    if (!std::isalnum(static_cast<unsigned char>(c))) {
                        tga::io::log::err::error("player_id must be alphanumeric");
                        std::exit(1);
                    }
                }
            } else if (arg == "-s" && i + 1 < argc) {
                if (server_set) tga::io::log::err::error("Parameter -s specified multiple times");
                server_set = true;
                server = argv[++i];
            } else if (arg == "-p" && i + 1 < argc) {
                if (port_set) tga::io::log::err::error("Parameter -p specified multiple times");
                port_set = true;
                if (!parse_uint(argv[++i], 1, 65535, port)) {
                    tga::io::log::err::error("Invalid port value");
                    std::exit(1);
                }
            } else if (arg == "-4") {
                force_ipv4 = true;
                ipv4_count++;
            } else if (arg == "-6") {
                force_ipv6 = true;
                ipv6_count++;
            } else if (arg == "-a") {
                strategy_a = true;
                strategy_a_count++;
            } else {
                tga::io::log::err::error("Unknown or incomplete argument: " + arg);
                std::exit(1);
            }
        }
        if (!player_id_set || !server_set || !port_set) {
            tga::io::log ::err::error("Missing required parameters: -u <player_id>, -s <server>, -p <port>");
            std::exit(1);
        }
        if (force_ipv4 && force_ipv6) {
            force_ipv4 = force_ipv6 = false;
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

static std::unique_ptr<ProgramArgs> make_args() {
    if (tga::config::server) {
        return std::make_unique<ServerArgs>();
    } else if (tga::config::client) {
        return std::make_unique<ClientArgs>();
    }
    
    return nullptr;
}

std::unique_ptr<ProgramArgs> args;
bool parsed = false;

} // anonymous namespace

/**
 * @brief Parses command line arguments.
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * @note hould be called before any other function in this namespace.
 */
void parse(int argc, char** argv) {
    assert(!parsed);
    args = make_args();
    args->parse(argc, argv);
    parsed = true;
}

/**
 * @brief Prints the parsed arguments.
 * @note MUST be called after `parse()`.
 */
void print() {
    assert(parsed);
    args->print();
}

namespace server {

/**
 * @brief returns M value (explained in README).
 * @note MUST be called after `parse()`.
 * @return M value.
 */
unsigned M() {
    assert(parsed);
    return static_cast<ServerArgs*>(args.get())->getM();
}

} // namespace server

namespace client {

} // namespace client

unsigned port() {
    assert(parsed);
    return args->getPort();
}

} // namespace args
} // namespace tga