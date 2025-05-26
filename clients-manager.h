#ifndef TGA_CLTMAN_H
#define TGA_CLTMAN_H

#include <string>

#include "netutils.h"

namespace tga {
namespace cltman {

using tga::net::SockAddrVariant;

unsigned requests_handled();
void register_client(const std::string &id, const SockAddrVariant &addr);
void deregister_client(const std::string &id, const SockAddrVariant &addr);

}
}

#endif // TGA_CLTMAN_H