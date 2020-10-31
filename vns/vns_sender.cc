#include <iostream>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <cstring>

#include "args.hh"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

int32_t main( int32_t argc, char *argv[]) {
    bool            res;
    sender_args     args;

    if (false == (res = parse_sender_args(argc, argv, args))) {
        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
