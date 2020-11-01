#ifndef __PACKET_SENDER_H__
#define __PACKET_SENDER_H__

#include <boost/random/exponential_distribution.hpp>
#include <boost/random/mersenne_twister.hpp>

#include "vns_args.hh"
#include "vns_utils.hh"

#define IS_DETERMINISTIC(x) (x == traffic_dist_type::DETERMINISTIC)
#define IS_BYTE_SWITCHER(x) (x == switching_type::BYTES)

template <class T>
class packet_sender {
    private:
        T                   &tcp_type;
        int32_t             ncycles;
        double_t            on_units,
                            off_units;
        switching_type      switcher;
        traffic_dist_type   traffic_dist;

        void update_dist(double_t &on, double_t &off, expo _on, expo _off);

    public:
        packet_sender(T &type, sender_args args);
        void run();
};

template <class T>
packet_sender<T>::packet_sender(T &type, sender_args args) :
    tcp_type(type),
    ncycles(args.cycles),
    on_units(args.on_units),
    off_units(args.off_units),
    switcher(args.switcher),
    traffic_dist(args.traffic_dist)
{ }

template <class T>
void packet_sender<T>::update_dist(double_t &on, double_t &off, expo _on, expo _off) {
    on = IS_DETERMINISTIC(traffic_dist) ? on_units : _on.dist_sample();
    off = IS_DETERMINISTIC(traffic_dist) ? off_units : _off.dist_sample();
}

template <class T>
void packet_sender<T>::run() {
    int32_t     fid = 1,
                seed = uniform_int_distribution<>()(PRNG()),
                sid = uniform_int_distribution<>()(PRNG());
    double_t    _off_units,
                _on_units;
    string      units;

    ran::mt19937 _prng(seed);
    expo on (1 / on_units, _prng);
    expo off (1 / off_units, _prng);

    while (fid <= ncycles) {

        update_dist(_on_units, _off_units, on, off);

        msleep(_off_units);

        tcp_type.send_data(_on_units, IS_BYTE_SWITCHER(switcher), fid, sid);

        units = IS_BYTE_SWITCHER(switcher) ? "bytes]" : "ms]";
        cout << "[sid:" << sid << "][fid:" << fid << "][on:" << _on_units << units << endl;
        fid += 1;
    }
}

#endif
