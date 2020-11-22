#undef NDEBUG

#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

#include "vns_eyenet.hh"

#define MAX_UPDATE_AMT 0.006
#define UPDATE_AMT 0.005

using namespace std;

eyenet::eyenet(double_t delta):
    fid(1),
    fid_counter(0),
    flow_sz(),
    last_update_dir(),
    nack(),
    nlost(),
    nprobe(),
    nsent(),
    pkts_rtt(),
    update_dir(),
    behaviour(),
    curr_intersend(),
    curr_ts(),
    delta(delta),
    default_delta(0.5),
    exp_incr(),
    intersend_rate(),
    last_ack_ts(),
    last_avg_send_rate(),
    last_delta_update_ts(),
    last_delta_loss_update_ts(),
    last_intersend(),
    last_intersend_rate(),
    last_rtt(),
    last_rtt_update_ts(),
    last_update_ts(),
    max_qd_est(),
    rtt_min(),
    ss_thres(),
    update_amt(),
    do_ss(false),
    loss_rtt(),
    ss(),
    unacked_pkts(),
    unacked_rtt(1.0),
    interarrival(0.03125),
    last_delta(1.0),
    rtt_sample(),
    interarrival_rank(0.05),
    reduce_on_loss(),
    rand_gen(),
    utility_type(CONSTANT),
    operation_type(DEFAULT),
    pkt_info(),
    cwnd(2),
    intersend(0),
    rto(1000)
{}

void eyenet::update_intersend() {
    double_t    rtt = rtt_sample.virgin_rtt;
    double_t    qd = rtt - rtt_min;
    double_t    t_cwnd;

    if (nack < (2 * nprobe - 1))
        return;

    if (qd == 0)
        t_cwnd = DOUBLE_MAX;
    else
        t_cwnd = rtt / (qd * delta);

    if (ss) {
        if (do_ss || t_cwnd == DOUBLE_MAX) {
            cwnd += 1;
            if (cwnd >= t_cwnd)
                ss = false;
        }
     } else {
         if (last_update_ts + rtt_sample.curr_rtt < curr_ts) {
             if (last_update_dir * update_dir > 0) {
                 if (update_amt < MAX_UPDATE_AMT)
                     update_amt += UPDATE_AMT;
                 else
                     update_amt = (int32_t) update_amt * 2;
             }
         } else {
             update_amt = 1.0;
             last_update_dir *= -1;
         }
         last_update_ts = curr_ts;
         pkts_rtt = update_dir = 0;
     }

    if (update_amt > cwnd * delta)
        update_amt /= 2;

    update_amt = max(update_amt, 1.0);
    ++pkts_rtt;

    if (cwnd < t_cwnd) {
        ++update_dir;
        cwnd += update_amt / (delta * cwnd);
    } else {
        --update_dir;
        cwnd -= update_amt / (delta * cwnd);
    }

    cwnd = max(2.0, cwnd);
    curr_intersend = 0.5 * rtt / cwnd;
    intersend = curr_intersend;
}

void eyenet::update_delta(bool loss __attribute((unused)),
                          double_t curr_rtt) {
    if (utility_type == AUTO) {
        if (!rtt_sample.mode_eyenet()) {
            operation_type = LOSS;
        } else {
            operation_type = DEFAULT;
        }
    }

    if (operation_type == DEFAULT && utility_type != COOP) {
        if (last_delta_update_ts == 0.0 ||
                (last_delta_loss_update_ts + curr_rtt < curr_ts)) {
            if (delta < default_delta)
                delta = default_delta;

            delta = min(delta, default_delta);
            last_delta_update_ts = curr_ts;
        }
    } else if (operation_type == LOSS || utility_type == COOP) {
        if (last_delta_update_ts == 0.0)
            delta = default_delta;

        if (loss && (last_delta_loss_update_ts + curr_rtt < curr_ts)) {
            delta *= 2;
            last_delta_loss_update_ts = curr_ts;
        } else {
            if (last_delta_update_ts + curr_rtt < curr_ts) {
                delta = 1 / (1 / delta + 1);
                last_delta_update_ts = curr_ts;
            }
        }

        delta = min(delta, default_delta);
    }
}

void eyenet::set_ts(double_t ts) {
    curr_ts = ts;
}

void eyenet::set_flow_sz(int32_t sz) {
    flow_sz = sz;
}

void eyenet::initialize() {
    if (nack) {
        cout << "=====================================================" << endl;
        cout << "Timestamp : " << curr_ts << endl;
        cout << "Slow start : " << ss << endl;
        cout << "Loss : " << (nlost * 100) / (nack + nlost) << endl;
        cout << "Minimum RTT : " << rtt_min << endl;
        cout << "Packet stats: " << endl;
        cout << "\tSent : " << nsent << endl;
        cout << "\tACKed : " << nack << endl;
        cout << "\tLost : " << nlost << endl;
    }

    intersend = 0;
    cwnd = nprobe;
    rtt_min = DOUBLE_MAX;
    rto = 1000;

    if (utility_type == CONSTANT)
        delta = 1;

    unacked_pkts.clear();
    unacked_rtt.clear();
    rtt_sample.clear();
    last_intersend = 0;
    curr_intersend = 0;
    interarrival_rank.clear();
    last_update_ts = 0;
    update_dir = 1;
    last_update_dir = 1;
    pkts_rtt = 0;
    update_amt = 1;
    intersend_rate = 0;
    last_intersend_rate = 0;
    last_rtt = 0;
    last_rtt_update_ts = 0;
    last_avg_send_rate = 0;
    nack = 0;
    nlost = 0;
    nsent = 0;
    operation_type = DEFAULT;
    flow_sz = 0;
    last_delta_update_ts = 0;
    last_delta_loss_update_ts = 0;
    max_qd_est = 0;
    reduce_on_loss.clear();
    loss_rtt = false;
    interarrival.clear();
    last_ack_ts = 0;
    exp_incr = 1;
    last_delta.clear();
    ss = true;
    ss_thres = 1E10;
}

void eyenet::on_ack(int32_t ack,
                    double_t recv_ts __attribute((unused)),
                    double_t send_ts __attribute((unused))) {

    if (curr_ts < send_ts) {
        cerr << "Send timestamp is in future" << endl;
        exit(EXIT_FAILURE);
    }

    bool        loss = false,
                reduce = false;
    int32_t     seq_no = ack - 1;
    double_t    curr_rtt = curr_ts - send_ts;

    rtt_sample.add(curr_rtt, curr_ts);
    rtt_min = rtt_sample.min_rtt;

    if (rtt_sample.virgin_rtt < rtt_min) {
        cerr << "Instantenous RTT smaller than minimum RTT" << endl;
        exit(EXIT_FAILURE);
    }

    if (last_ack_ts) {
        interarrival.update(curr_ts - last_ack_ts);
        interarrival_rank.add(curr_ts - last_ack_ts);
    }

    last_ack_ts = curr_ts;

    update_delta(false, curr_rtt);
    update_intersend();

    if (unacked_pkts.count(seq_no)) {
        int32_t tmp_seq = -1;

        for (auto pkt : unacked_pkts) {
            if (tmp_seq > pkt.first) {
                cerr << "Malformed unacked packets" << endl;
                exit(EXIT_FAILURE);
            }

            tmp_seq = pkt.first;

            if (pkt.first > seq_no) break;

            last_intersend = pkt.second.intersend;
            last_intersend_rate = pkt.second.intersend_rate;
            last_rtt = pkt.second.rtt;
            last_rtt_update_ts = pkt.second.sent_ts;
            last_avg_send_rate = pkt.second.last_avg_send_rate;

            if (pkt.first < seq_no) {
                ++nlost;
                loss = true;
                reduce |= reduce_on_loss.update(true, curr_ts, rtt_sample.curr_rtt);
            }
            unacked_pkts.erase(pkt.first);
        }
    }

    if (loss) {
        update_delta(true, 0);
    }

    reduce |= reduce_on_loss.update(false, curr_ts, rtt_sample.curr_rtt);
    if (reduce) {
        cwnd *= 0.8;
        cwnd = max(2.0, cwnd);
        curr_intersend = 0.5 * rtt_sample.virgin_rtt / cwnd;
        intersend = curr_intersend;
    }

    ++nack;
}

void eyenet::on_pkt_sent(int32_t seq_no) {
    unacked_pkts[seq_no] = {
        curr_intersend,
        intersend_rate,
        unacked_pkts.size() / (curr_ts - last_rtt_update_ts),
        rtt_sample.virgin_rtt,
        curr_ts
    };

    unacked_rtt.set(rtt_sample.virgin_rtt);
    for (auto pkt : unacked_pkts) {
        if (curr_ts - pkt.second.sent_ts > unacked_rtt) {
            unacked_rtt.update(curr_ts - pkt.second.sent_ts);
            last_intersend = pkt.second.intersend;
            last_intersend_rate = pkt.second.intersend_rate;
        }
        break;
    }

    ++nsent;

    intersend = curr_intersend;
}
