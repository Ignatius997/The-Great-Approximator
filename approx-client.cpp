#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "MessageCombinators.h"
#include "communication.h"

using tga::msg::HelloMessage;

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if(tga::config::debug) tga::args::print();

    tga::comm::setup();
    tga::comm::send_message(tga::comm::client::get_sockfd(),
                            HelloMessage(tga::args::client::player_id()));

    while (true) {
        
    }
}