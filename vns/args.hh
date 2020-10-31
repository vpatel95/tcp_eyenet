#ifndef __ARGS_H__
#define __ARGS_H__

#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

using std::cout;
using std::cerr;
using std::string;
using std::endl;

bool LOGS = false;
string LOG_FILE;

enum congestion_type {
    DEFAULT,
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

sender_args::sender_args() :
    cycles(1),
    dst_port(8001),
    src_port(8001),
    on_units(5000),
    off_units(5000),
    bn_train(1),
    weight(0.5),
    link_rate(2500),
    log_file(),
    dst_ip(),
    traffic_dist(EXPONENTIAL),
    switcher(TIME),
    logging(),
    cong(DEFAULT)
{ }

bool parse_sender_args( int32_t argc, char *argv[], sender_args &args ) {
    try {
        po::options_description opts("Allowed Options");
        opts.add_options()
            ("help", "Display this message")
            ("cycles", po::value<int32_t>(&args.cycles)->default_value(1), "Set number of cycles")
            ("dst-port", po::value<int32_t>(&args.dst_port)->default_value(8001), "Set the destination port")
            ("src-port", po::value<int32_t>(&args.src_port)->default_value(8001), "Set the source port")
            ("on-unit", po::value<int32_t>(&args.on_units)->default_value(5000),
                "Set on units for deterministic on-off traffic distribution")
            ("off-unit", po::value<int32_t>(&args.off_units)->default_value(5000),
                "Set off units for deterministic on-off traffic distribution")
            ("bn-train", po::value<int32_t>(&args.bn_train)->default_value(1),
                "Set packet train to determine bottleneck")
            ("weight", po::value<double_t>(&args.weight)->default_value(0.5), "Set the weight to control rate")
            ("link-rate", po::value<double_t>(&args.link_rate)->default_value(2500), "Set the link rate")
            ("log-file", po::value<string>(&args.log_file), "Set the logging file")
            ("dst-ip", po::value<string>(&args.dst_ip)->required(), "Set the destination ip")
            ("traffic-dist", po::value<string>()->default_value("exponential"), "Set traffic on-off type")
            ("switcher", po::value<string>()->default_value("time"), "Set traffic switcher")
            ("cong", po::value<string>()->default_value("kernel"), "Set the congestion algorithm")
        ;

        po::variables_map opt_map;
        po::store(po::parse_command_line(argc, argv, opts), opt_map);

        if (opt_map.count("help")) {
            cout << opts << endl;
            return false;
        }

        po::notify(opt_map);

        if (opt_map.count("traffic-dist")) {
            if (opt_map["traffic-dist"].as<string>() == "exponential") {
                args.traffic_dist = EXPONENTIAL;
            } else if (opt_map["traffic-dist"].as<string>() == "deterministic") {
                args.traffic_dist = DETERMINISTIC;
            } else {
                cout << "Unknown traffic distribution. Setting it to default 'exponential'" << endl;
                args.traffic_dist = EXPONENTIAL;
            }
        }

        if (opt_map.count("switcher")) {
            if (opt_map["switcher"].as<string>() == "time") {
                args.switcher = TIME;
            } else if (opt_map["switcher"].as<string>() == "bytes") {
                args.switcher = BYTES;
            } else {
                cout << "Unknown switching type. Setting it to default 'time'" << endl;
                args.switcher = TIME;
            }
        }

        if (opt_map.count("cong")) {
            if (opt_map["cong"].as<string>() == "kernel") {
                args.cong = DEFAULT;
            } else if (opt_map["cong"].as<string>() == "eyenet") {
                args.cong = EYENET;
            } else {
                cout << "Unknown congestion control. Setting it to default 'kernel'" << endl;
                args.cong = DEFAULT;
            }
        }
    } catch (po::required_option &e) {
        cerr << e.what() << endl;
        return false;
    } catch (...) {
        cerr << "Unknown error" << endl;
        return false;
    }

    return true;
}

#endif
