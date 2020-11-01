#include <iostream>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <cstring>

#include "default.hh"
#include "vns_args.hh"
#include "vns_packet_sender.hh"
#include "vns_transport.hh"

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

    switch (args.cong) {
        case congestion_type::DEFAULT: {
            default_cong conn(args.dst_ip);
            packet_sender<default_cong> pkt_sender (conn, args);
            pkt_sender.run();
            break;
        }
        case congestion_type::EYENET: {
            break;
        }
        default:
            break;
    }
    return EXIT_SUCCESS;
}
