/**
 * This is client's program for The Great Approximator game.
 * 
 * Author: Ignacy Pernach
 * Date: 06.06.2025
 */

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "MessageCombinators.h"
#include "communication.h"

using tga::msg::HelloMessage;
using tga::msg::MsgPtr;

using tga::comm::ReceiveInfo;
using tga::comm::client::clear_revents;
using tga::comm::client::poll_events;
using tga::comm::client::handle_poll_event_from_server;
using tga::comm::client::handle_poll_event_from_stdin;

using tga::io::log::err::error;

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if(tga::config::debug) tga::args::print();

    tga::comm::setup();

    // Present yourself to server.
    MsgPtr msg = std::make_unique<HelloMessage>(tga::args::client::player_id());
    tga::comm::client::prepare_to_send(std::move(msg));

    while (true) {
        clear_revents();
        
        int poll_status = poll_events();
        if (poll_status < 0) { // Fail.
            error("poll");
            exit(1);
        } else if (poll_status > 0) { // Success.
            std::optional<int> exit_code = handle_poll_event_from_server();
            if (exit_code.has_value()) {
                exit(exit_code.value());
            }

            if (!tga::args::client::default_strategy()) {
                handle_poll_event_from_stdin();
            }
        }
    }

    return 0;
}