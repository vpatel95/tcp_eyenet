#include <iostream>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <cstring>

#include "default.hh"
#include "vns_args.hh"
#include "vns_eyenet.hh"
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
        case congestion_type::LINUX: {
            default_cong conn(args.dst_ip);
            packet_sender<default_cong> pkt_sender (conn, args);
            pkt_sender.run();
            break;
        }
        case congestion_type::EYENET: {
            eyenet cong_control(1.0);
            tcp_transport<eyenet> conn(cong_control, args);
            packet_sender< tcp_transport<eyenet> > pkt_sender(conn, args);
            pkt_sender.run();
            break;
        }
        default:
            break;
    }
    return EXIT_SUCCESS;
}
