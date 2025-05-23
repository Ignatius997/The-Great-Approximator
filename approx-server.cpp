#include <iostream>

#include "io.h"
#include "Rational.h"
#include "args.h"
#include "config.h"
#include "communication.h"
#include "clients-manager.h"

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if (tga::config::debug) tga::args::print();

    tga::comm::setup();

    while (true) {
        const auto &info = tga::comm::receive_message();
        tga::comm::handle_message(info);
        if (tga::cltman::requests_handled() == tga::args::server::M()) {
            tga::comm::end();
            break;
        }
    }

    return 0;
}