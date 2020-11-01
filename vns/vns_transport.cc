#include <assert.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <thread>

#include "vns_transport.hh"

using namespace std;
using chrono::duration;
using chrono::duration_cast;

#define NEG -1
#define ALPHA (1/16.0)
#define PKT_SZ 1440
#define BUF_SZ (sizeof(char) * PKT_SZ)
#define PAYLOAD (PKT_SZ - HDR_SZ)
#define clk std::chrono::high_resolution_clock

double_t get_curr_ts(clk::time_point &start_ts) {
    clk::time_point curr_ts = clk::now();
    return duration_cast<duration<double_t>>(curr_ts - start_ts).count()*1000;
}

template <class T>
tcp_transport<T>::tcp_transport(T cong, string dip, int32_t dport, int32_t sport, int32_t bn_train) :
    bn_train(bn_train), last_s_ts(0.0), last_r_ts(0.0), last_ack_ts(0.0),
    cong(cong), sk(), type(SENDER), dip(dip), dport(dport), sport(sport),
    largest_ack(NEG), trans_time_aggr(0), delay_aggr(0), npkts(0), nbytes(0)
{
    sk.bind_sk(dip, dport, sport);
}

template <class T> void tcp_transport<T>::handshake() {
    int32_t         rc = 0;
    packet_header   hdr,
                    ack_hdr;
    char            buf[PKT_SZ];
    double_t        calc_rtt,
                    _last_s_ts = -1E9;
    bool            corrupt = false;
    sockaddr_in     sk_addr;

    memset(&buf, '#', BUF_SZ);
    buf[PKT_SZ - 1] = '\0';

    hdr.seq_num = NEG;
    hdr.fid = NEG;
    hdr.sid = NEG;
    hdr.send_ts = NEG;
    hdr.recv_ts = NEG;

    clk::time_point start_ts;
    start_ts = clk::now();

    while(true) {
        double_t curr_ts = get_curr_ts(start_ts);
        if (_last_s_ts < curr_ts - 200) {
            memcpy(buf, &hdr, HDR_SZ);
            sk.send_data(buf, HDR_SZ * 2, NULL);

            _last_s_ts = curr_ts;
            corrupt = true;
        }

        if (0 == (rc = sk.recv_data(buf, PKT_SZ, 200, sk_addr))) {
            cerr << "Failed to establish connection with peer." << endl;
            continue;
        }

        memcpy(&ack_hdr, buf, HDR_SZ);
        if ((ack_hdr.seq_num != NEG) ||
            (ack_hdr.fid != NEG) ||
            (ack_hdr.send_ts != -1E9) ||
            (ack_hdr.sid != NEG)) {
            continue;
        }

        calc_rtt = get_curr_ts(start_ts) - _last_s_ts;
        break;
    }

    if (!corrupt)
        cong.set_min_rtt(calc_rtt);

    cout << "Connection Established" << endl;
}

void create_header(packet_header *hdr, int32_t sq, int32_t fid, int32_t sid, double_t ts) {
    hdr->seq_num = sq;
    hdr->fid = fid;
    hdr->sid = sid;
    hdr->send_ts = ts;
    hdr->recv_ts = 0;
}

template <class T>
void tcp_transport<T>::send_data( double_t fsz, bool byte_switcher, int32_t fid, int32_t sid) {
    char            buf[PKT_SZ];
    int32_t         seq_num = 0;
    double_t        curr_ts = 0.0,
                    timeout = 0.0,
                    rate_est = 0.0,
                    tput = 0.0,
                    delay = 0.0;
    packet_header   hdr,
                    ack_hdr;

    memset(buf, '#', BUF_SZ);
    buf[PKT_SZ - 1] = '\0';

    largest_ack = NEG;

    handshake();

    clk::time_point start_ts = clk::now();
    curr_ts = get_curr_ts(start_ts);
    last_s_ts = curr_ts;
    last_ack_ts = curr_ts;

    curr_ts = get_curr_ts(start_ts);
    cong.set_curr_ts(curr_ts);
    cong.init();

#define _switch_sz (byte_switcher ? npkts * PAYLOAD : curr_ts)
    while (_switch_sz < fsz) {
        curr_ts = get_curr_ts(start_ts);
        if (curr_ts - last_ack_ts > 2000) {
            cout << "[" << __FILE__ << "] ::: Timeout occured" << endl;

            // To reduce the frequency of timeouts
            // we reset the acks and ack ts
            cong.set_curr_ts(curr_ts);
            cong.init();

            largest_ack = seq_num - 1;
            last_s_ts = curr_ts;
            last_ack_ts = curr_ts;
        }

        while (((seq_num < largest_ack + cong.cwnd() + 1) &&
                (last_s_ts + cong.intersend_time() * bn_train <= curr_ts)) ||
                (seq_num % bn_train != 0)) {

            create_header(&hdr, seq_num, fid, sid, curr_ts);
            memcpy(buf, &hdr, HDR_SZ);
            sk.send_data(buf, PKT_SZ, NULL);
            last_s_ts += cong.intersend_time();

            if (seq_num %bn_train == 0) {
                cong.set_curr_ts(curr_ts);
                cong.on_pkt_sent(hdr.seq_num / bn_train);
            }

            seq_num += 1;
        }

        if ((curr_ts - last_s_ts >= cong.intersend_time() * bn_train) ||
                seq_num >= largest_ack + cong.cwnd()) {
            last_s_ts = curr_ts;
        }

        curr_ts = get_curr_ts(start_ts);
        timeout = last_s_ts + 1000;

        if (cong.cwnd() > 0)
            timeout = min(1000, last_s_ts + cong.intersend_time() * bn_train - curr_ts);

        sockaddr_in addr;
        if(0 == sk.recv_data(buf, PKT_SZ, timeout, addr)) {
            curr_ts = get_curr_ts(start_ts);
            if (curr_ts > last_s_ts + cong.timeout())
                cong.on_timeout();
            continue;
        }

        memcpy(&ack_hdr, buf, HDR_SZ);

        // Add this logic on receiver;
        ack_hdr.seq_num++;

        if (ack_hdr.sid != sid || ack_hdr.fid != fid) continue;

        curr_ts = get_curr_ts(start_ts);
        last_ack_ts = curr_ts;

        if ((ack_hdr.seq_num - 1) * bn_train != 0 && last_r_ts != 0) {
            if (rate_est == 0)
                rate_est = 1 * (curr_ts - last_r_ts);
            else
                rate_est = (((1 - ALPHA) * rate_est) + (ALPHA * (curr_ts - last_r_ts)));

            if (ack_hdr.seq_num > 2 * bn_train)
                cong.on_rate_est(1000/rate_est);
        }

        last_r_ts = curr_ts;

        delay_aggr += curr_ts - ack_hdr.send_ts;
        nbytes += PAYLOAD;
        npkts += 1;

        if ((ack_hdr.seq_num - 1) && bn_train == 0) {
            cong.set_curr_ts(curr_ts);
            cong.on_ack(ack_hdr.seq_num / bn_train, ack_hdr.recv_ts, ack_hdr.send_ts);
        }

        largest_ack = max(largest_ack, ack_hdr.seq_num);
    }

    curr_ts = get_curr_ts(start_ts);
    cong.set_curr_ts(curr_ts);
    cong.close();

    trans_time_aggr += curr_ts;

    tput = nbytes / (trans_time_aggr / 1000.0);
    delay = delay_aggr / npkts;

    cout << "Statistics :" << endl;
    cout << "\tTPUT : " << tput << "bps" << endl;
    cout << "\tLAT : " << delay << "ms/pkt" << endl;
}
