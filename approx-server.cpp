#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "communication.h"
#include "clients-manager.h"
#include "utils.h"

using tga::utils::ReceiveInfo;

using tga::comm::setup;
using tga::comm::server::clear_revents;
using tga::comm::server::update_timeouts;
using tga::comm::server::handle_poll_event;
using tga::comm::server::messages_to_receive;
using tga::comm::server::poll_structure_size;
using tga::comm::server::connection_exists;
using tga::comm::server::new_ipv4_clients;
using tga::comm::server::new_ipv6_clients;
using tga::comm::server::new_clients;
using tga::comm::server::poll_events;
using tga::comm::server::end;

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if (tga::config::debug) tga::args::print();

    setup();

    while (messages_to_receive() > 0) {
        ReceiveInfo rinfo;
        clear_revents();
        update_timeouts();
        
        // TODO Maybe handle Ctrl-C like in echo-server-nonblocking.c

        int poll_status = poll_events();
        if (poll_status < 0) { // fail
            tga::io::log::err::error("poll");
            exit(1);
        } else if (poll_status == 0) { // timeout
            tga::io::log::err::error("poll timeout");
            exit(1);
        } else { // success
            if (new_ipv4_clients()) new_clients(AF_INET);

            if (new_ipv6_clients()) new_clients(AF_INET6);

            for (size_t i = 2; i < poll_structure_size(); ++i) {
                if (connection_exists(i)) {
                    handle_poll_event(i, rinfo);
                    if (messages_to_receive() == 0) {
                        end();
                        break;
                    }
                }
            }
        }
    }

    return 0;
}