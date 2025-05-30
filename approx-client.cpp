#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "MessageCombinators.h"
#include "communication.h"

using tga::comm::ReceiveInfo;

using tga::msg::HelloMessage;
using tga::msg::MsgPtr;

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if(tga::config::debug) tga::args::print();

    tga::comm::setup();

    // Send a HELLO message to the server with the player's ID to identify yourself.
    MsgPtr msg = std::make_unique<HelloMessage>(tga::args::client::player_id());
    tga::comm::client::send_message(std::move(msg));

    // Receive SCORING message from the server to start the game.
    tga::comm::client::receive_message();

    while (true) {
        
    }
}