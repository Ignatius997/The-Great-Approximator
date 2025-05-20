#include <iostream>

#include "log.h"
#include "io.h"
#include "Rational.h"
#include "log-server.h"
#include "args.h"
#include "global.h"
#include "communication.h"
#include "clients-manager.h"
#include "netutils.h"

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if (tga::global::debug) tga::args::print();

    tga::net::setup();

    while (true) {
        auto info = tga::comm::receive_message();
        tga::comm::handle_message(info);
        if (tga::cltman::requests_handled() == tga::args::server::M()) {
            tga::comm::end();
            break;
        }
    }

    return 0;
}