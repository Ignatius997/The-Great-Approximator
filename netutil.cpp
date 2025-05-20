#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <memory>

#include "netutils.h"
#include "err.h"

namespace tga {
namespace net {

/**
 * @brief Resolves a hostname to an IPv4 sockaddr_in structure for TCP connections.
 *
 * This function uses getaddrinfo to resolve the given host name or IP address
 * to an IPv4 address and fills a sockaddr_in structure with the resolved address
 * and the specified port (in network byte order).
 * @note Function based on function from laboratories.
 *
 * @param host Hostname or IPv4 address as a string.
 * @param port TCP port number (host byte order).
 * @return sockaddr_in structure ready to use for connect() or bind().
 *
 * @throws std::runtime_error if address resolution fails.
 */
struct sockaddr_in get_server_address(const std::string &host, unsigned port) {
    struct addrinfo hints;
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *address_result = nullptr;
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> address_guard(nullptr, freeaddrinfo);

    int errcode = getaddrinfo(host.c_str(), nullptr, &hints, &address_result);
    address_guard.reset(address_result);

    if (errcode != 0) {
        tga::err::error(std::string("getaddrinfo: ") + gai_strerror(errcode));
        exit(1);
    }

    struct sockaddr_in send_address;
    send_address.sin_family = AF_INET;
    send_address.sin_addr.s_addr =
        reinterpret_cast<sockaddr_in*>(address_result->ai_addr)->sin_addr.s_addr;
    send_address.sin_port = htons(static_cast<uint16_t>(port));

    return send_address;
}
} // namespace net
} // namespace tga