#ifndef __TCP_EYENET_H__
#define __TCP_EYENET_H__

#include <chrono>
#include <functional>
#include <iostream>
#include <map>

#include "vns_utils.hh"

enum operations {
    DEFAULT,
    TCP,
    LOSS
};

enum utility {
    CONSTANT,
    AUTO,
    COOP
};

typedef struct packet_info {
    double_t    intersend,
                intersend_rate,
                last_avg_send_rate,
                rtt,
                sent_ts;
} packet_info;

class eyenet {
    private:
        int32_t                 fid,
                                fid_counter,
                                flow_sz,
                                last_update_dir,
                                nack,
                                nlost,
                                nprobe,
                                nsent,
                                pkts_rtt,
                                update_dir;

        double_t                behaviour,
                                curr_intersend,
                                curr_ts,
                                delta,
                                default_delta,
                                exp_incr,
                                intersend_rate,
                                last_ack_ts,
                                last_avg_send_rate,
                                last_delta_update_ts,
                                last_delta_loss_update_ts,
                                last_intersend,
                                last_intersend_rate,
                                last_rtt,
                                last_rtt_update_ts,
                                last_update_ts,
                                max_qd_est,
                                rtt_min,
                                ss_thres,
                                update_amt;

        bool                    do_ss,
                                loss_rtt,
                                ss;

        map<int32_t, packet_info>  unacked_pkts;

        ewma                    unacked_rtt,
                                interarrival,
                                last_delta;

        rtt_win                 rtt_sample;
        data_rank               interarrival_rank;
        loss_reduce             reduce_on_loss;
        random_generator        rand_gen;
        utility                 utility_type;
        operations              operation_type;
        packet_info             pkt_info;

        void update_intersend();
        void update_delta(bool loss, double_t curr_rtt);


    public:
        double_t    cwnd,
                    intersend,
                    rto;

        eyenet(double_t delta);
        void initialize();
        void on_ack(int32_t ack, double_t recv_ts, double_t send_ts);
        void on_pkt_sent(int32_t seq_no);
        void set_ts(double_t ts);
        void set_flow_sz(int32_t sz);
        void on_to() {}
        void on_rate_est(double_t link_rate_est __attribute((unused))) {}
        void close() {}
};

#endif
