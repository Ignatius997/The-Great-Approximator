#include <iostream>

#include "log.h"
#include "err.h"
#include "io.h"
#include "Rational.h"
#include "log-server.h"
#include "args.h"
#include "global.h"

void end() {
    // send SCORING messages to clients
    // close connections
    // exit
}

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    if (tga::global::debug) tga::args::print();

    while (true) {
        info = receive_message();
        handle_message(info);
        if (sent_count == tga::args::get_m()) {
            end();
            break;
        }
    }

    return 0;
}