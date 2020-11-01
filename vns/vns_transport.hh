#ifndef __TCP_TRANSPORT_H__
#define __TCP_TRANSPORT_H__


#include "vns_base.hh"


enum node {
    SENDER,
    RECEIVER
};

template <class T>
class tcp_transport {
    private:
        T               cong;
        node            type;
        eyenet_socket   sk;
        string          dip;
        int32_t         dport,
                        sport,
                        bn_train;

        double_t        last_s_ts,
                        last_r_ts,
                        last_ack_ts,
                        delay_aggr,
                        trans_time_aggr;

        int32_t         largest_ack,
                        npkts,
                        nbytes;

        void handshake();

    public:
        tcp_transport(T cong, string dip, int32_t dport, int32_t sport, int32_t bn_train);

        void send_data (double_t fsz, bool byte_switcher, int32_t fid, int32_t sid);
        void listen_data();
};

#endif
