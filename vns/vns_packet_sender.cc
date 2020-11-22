/* TEMPLATE functions need to be defined in the header file itself


 #include "vns_packet_sender.hh"

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

 */
