#include <iostream>
#include <unistd.h> // TODO Delete for sleep

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "communication.h"
#include "netutils.h"


using tga::net::ReceiveInfo;
using tga::io::log::err::error;
using tga::config::debug;

using tga::args::parse;
using tga::args::print;

using tga::comm::setup;
using tga::comm::server::clear_revents;
using tga::comm::server::update_timeouts;
using tga::comm::server::handle_timeouts;
using tga::comm::server::handle_poll_event;
using tga::comm::server::get_messages_to_receive;
using tga::comm::server::poll_structure_size;
using tga::comm::server::connection_exists;
using tga::comm::server::new_ipv4_clients;
using tga::comm::server::new_ipv6_clients;
using tga::comm::server::new_clients;
using tga::comm::server::poll_events;
using tga::comm::server::send_scores;
using tga::comm::server::should_send_scores;

int main(int argc, char* argv[]) {
    parse(argc, argv);
    if (debug) print();

    setup();

    // TODO Czy pętla ma być nieskończona?
    while (true) {
        clear_revents();
        
        // TODO Maybe handle Ctrl-C like in echo-server-nonblocking.c

        int poll_status = poll_events();
        if (poll_status < 0) { // Fail.
            error("poll");
            exit(1);
        } else if (poll_status == 0) { // Timeout.
            update_timeouts();
            handle_timeouts();

            // NOTE: Explanation, why sending scores is here in documentation.
            // if (should_send_scores()) {
            //     send_scores();
            // }
        } else { // success
            update_timeouts();
            handle_timeouts();

            if (new_ipv4_clients()) new_clients(AF_INET);

            if (new_ipv6_clients()) new_clients(AF_INET6);

            for (size_t i = 2; i < poll_structure_size(); ++i) {
                if (connection_exists(i)) {
                    handle_poll_event(i);
                }
            }
        }
    }

    return 0;
}