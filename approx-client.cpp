#include <iostream>

#include "log.h"
#include "io.h"
#include "Rational.h"
#include "log-client.h"
#include "args.h"

int main(int argc, char* argv[]) {
    tga::args::parse(argc, argv);
    tga::args::print();
}