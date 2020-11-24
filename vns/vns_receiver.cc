#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "vns_base.hh"

using namespace std;

#define BUF_SZ 15000
#define clk chrono::high_resolution_clock

void ack_packets(eyenet_socket &sk) {
    char        buf[BUF_SZ];
    sockaddr_in addr;

    clk::time_point start_ts = clk::now();

    while(true) {
        int32_t recv_pkt = -1;
        while (recv_pkt == -1) {
            recv_pkt = sk.recv_data(buf, BUF_SZ, -1, addr);
        }

        packet_header *hdr = (packet_header *)buf;
        hdr->recv_ts = chrono::duration_cast<chrono::duration<double_t>>(clk::now() - start_ts).count()*1000;
        sk.send_data(buf, sizeof(packet_header), &addr);
        cout << "[fid:" << hdr->fid << "] Acked pkt_seqno " << hdr->seq_num << endl;
    }
}

int32_t main(int32_t argc, char *argv[]) {
    int32_t         port = 8001;
    eyenet_socket   sk;

    if (argc == 2)
        port = atoi(argv[1]);

    sk.bind_sk(port);
    cout << "Listening on port " << port << endl;

    ack_packets(sk);

    return EXIT_SUCCESS;
}
