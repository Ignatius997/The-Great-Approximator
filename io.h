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
        void game_end(const std::vector<std::pair<std::string, std::string>>& results);
        void custom(const std::string &message);

        namespace server {
            void new_client(const std::string& ip, uint16_t port);
            void client_known(const std::string& ip, uint16_t port, const std::string& player_id);
            void get_coefficients(const std::string& player_id, const std::vector<std::string>& coeffs);
            void put_value(const std::string& player_id, const std::string& value, int point, const std::vector<std::string>& state);
            void send_state(const std::vector<std::string>& state, const std::string& player_id);
            void client_disconnected(const std::string& ip, uint16_t port, const std::string& player_id);
            
            // NOTE One są do debugowania, bo nie chce mi się na razie zajmowac wyspecyfikowanymi wiadomościami
            void received(const std::string &message, const std::string &player_id, const SockAddrVariant &addr);
            void sent(const std::string &message, const std::string &player_id, const SockAddrVariant &addr);
        }

        namespace client {
            void connected_to(const std::string& ip, uint16_t port);
            void received_coefficients(const std::vector<std::string>& coeffs);
            void putting_value(const std::string& value, int point);
            void received_state(const std::vector<std::string>& state);
            
            void server_disconnected();

            // NOTE One są do debugowania, bo nie chce mi się na razie zajmowac wyspecyfikowanymi wiadomościami
            void received(const std::string &message);
            void sent(const std::string &message);
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