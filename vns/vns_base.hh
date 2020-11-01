#ifndef __TCP_BASE_H__
#define __TCP_BASE_H__

#include <inttypes.h>
#include <cmath>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

using std::string;

#define HDR_SZ sizeof(packet_header)

typedef struct packet_header {
    int32_t     seq_num,
                fid,
                sid;
    double_t    send_ts,
                recv_ts;
} packet_header;

class eyenet_socket {
    private:
        int32_t     sk,
                    dport,
                    sport;
        string      dip;
        bool        is_bound;

    public:
        eyenet_socket();

        int32_t bind_sk(string dip, int32_t dport, int32_t sport);
        int32_t bind_sk(int32_t sport);

        ssize_t send_data(const char *data, ssize_t sz, sockaddr_in *dst_addr);
        ssize_t send_data(const char *data, ssize_t sz, string dip, int32_t dport);
        int32_t recv_data(char *buf, int32_t sz, int32_t timeout, sockaddr_in &sk_addr);

        static void decode_sk_addr(sockaddr_in sk_addr, string &ip, int32_t &port);
        static string decode_sk_addr(sockaddr_in sk_addr);
};

#endif
