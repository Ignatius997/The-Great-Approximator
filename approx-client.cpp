#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "netutils.h"
#include "MessageCombinators.h"
#include "communication.h"

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if(tga::config::debug) tga::args::print();

    tga::net::setup();
    tga::comm::send_message(tga::net::client::get_sockfd(), tga::msg::HelloMessage(tga::args::client::player_id()));
}