#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "MessageCombinators.h"
#include "communication.h"
#include <unistd.h> // TODO for sleep, remove later

using tga::msg::HelloMessage;
using tga::msg::MsgPtr;

using tga::comm::ReceiveInfo;
using tga::comm::client::clear_revents;
using tga::comm::client::poll_events;
using tga::comm::client::handle_poll_event;

using tga::io::log::err::error;

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if(tga::config::debug) tga::args::print();

    tga::comm::setup();
    sleep(1); // TODO Remove this later
    // Send a HELLO message to the server with the player's ID to identify yourself.
    MsgPtr msg = std::make_unique<HelloMessage>(tga::args::client::player_id());
    tga::comm::client::prepare_to_send(std::move(msg));

    // TODO Czy pętla ma być nieskończona?
    while (true) {
        clear_revents();
        
        // TODO Maybe handle Ctrl-C like in echo-server-nonblocking.c

        int poll_status = poll_events();
        if (poll_status < 0) { // Fail.
            error("poll");
            exit(1);
        } else if (poll_status > 0) { // Success.
            handle_poll_event(0); // Poll event for server.
            
            if (!tga::args::client::default_strategy()) {
                handle_poll_event(1); // Poll event for stdin.
            }
        }
    }

    return 0;
}