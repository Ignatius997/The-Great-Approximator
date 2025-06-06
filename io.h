#ifndef TGA_IO_H
#define TGA_IO_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <netinet/in.h>

#include "netutils.h"

namespace tga {
namespace io {

using tga::net::SockAddrVariant;

namespace log {
    namespace err {
        void error(const std::string &error_description);
        
        namespace server {
            void message(const std::string &message,
                const std::string &player,
                const SockAddrVariant &addr);
            void input(const std::string &input);
        }

        namespace client {
            void message(const std::string &message);
        }
    }

    namespace info {
        void custom(const std::string &message);
        void sent(const std::string &message, const std::string &player_id);

        namespace server {
            void new_client(const std::string& ip, uint16_t port);
            void hello(const std::string& ip, uint16_t port, const std::string& player_id);
            void put(const std::string& player_id, int point, const std::string& value, const std::vector<std::string>& state);
            void client_disconnected(const std::string& ip, uint16_t port, const std::string& player_id);
        }

        namespace client {
            void connected_to(const std::string& ip, uint16_t port);
            void coeff(const std::vector<std::string>& coeffs);
            void state(const std::vector<std::string>& coeffs);
            void scoring(const std::vector<std::string>& scores);
        }
    }
}

namespace file {
    void set_coeffs_file(const std::string& coeffs_file);
    std::string read_coeffs();
}

} // namespace io
} // namespace tga

#endif // TGA_IO_H