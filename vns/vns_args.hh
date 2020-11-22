#ifndef __ARGS_H__
#define __ARGS_H__

#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

using std::cout;
using std::cerr;
using std::string;
using std::endl;

enum congestion_type {
    LINUX,
    EYENET
};

enum switching_type {
    TIME,
    BYTES
};

enum traffic_dist_type {
    EXPONENTIAL,
    DETERMINISTIC
};

typedef struct sender_args {
    int32_t             cycles,
                        dst_port,
                        src_port,
                        on_units,
                        off_units,
                        bn_train;
    double_t            weight,
                        link_rate;
    string              log_file,
                        dst_ip;
    traffic_dist_type   traffic_dist;
    switching_type      switcher;
    bool                logging;
    congestion_type     cong;

    sender_args();
} sender_args;

bool parse_sender_args( int32_t argc, char *argv[], sender_args &args );

#endif
