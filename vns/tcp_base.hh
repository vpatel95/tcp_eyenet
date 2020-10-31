#ifndef __TCP_BASE_H__
#define __TCP_BASE_H__

#include <cassert>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <math.h>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>

using std::cerr;
using std::endl;
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

eyenet_socket::eyenet_socket() :
    sk(-1),
    dport(),
    sport(),
    dip(),
    is_bound(false)
{
    sk = socket(AF_INET, SOCK_DGRAM, 0);
}

int32_t eyenet_socket::bind_sk(string dst_ip, int32_t dst_port, int32_t src_port) {
    int32_t     rc = 0;
    dip = dst_ip;
    dport = dst_port;
    sport = src_port;

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(sport);

    if (0 > (rc = bind(sk, (struct sockaddr *)&saddr, sizeof(saddr)))) {
        cerr << "Error (" << errno << ") while binding the socket on port " << sport << endl;
        is_bound = false;
        return rc;
    }

    is_bound = true;
    return rc;
}

int32_t eyenet_socket::bind_sk(int32_t port) {
    int32_t     rc = 0;
    sport = port;

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(sport);

    if (0 > (rc = bind(sk, (struct sockaddr *)&saddr, sizeof(saddr)))) {
        cerr << "Error (" << errno << ") while binding the socket on port " << sport << endl;
        is_bound = false;
        return rc;
    }

    is_bound = true;
    return rc;
}

ssize_t eyenet_socket::send_data(const char *data, ssize_t sz, sockaddr_in *daddr) {
    int32_t     rc = 0;
    sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    if (daddr == NULL) {
        assert(is_bound);
        dst_addr.sin_port = htons(dport);
        if (0 == (rc = inet_aton(dip.c_str(), &dst_addr.sin_addr))) {
            cerr << "Error in inet_aton while sending data.\n\tIP : " << dip << endl;
            cerr << "Error Code : " << errno;
        }
    } else {
        dst_addr.sin_port = ((struct sockaddr_in *)daddr)->sin_port;
        dst_addr.sin_addr = ((struct sockaddr_in *)daddr)->sin_addr;
    }

    if (-1 == (rc = sendto(sk, data, sz, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr)))) {
        cerr << "Error while sending data. Error code : " << errno << endl;
    }

    return rc;

}

ssize_t eyenet_socket::send_data(const char *data, ssize_t sz, string dst_ip, int32_t dport) {
    int32_t     rc = 0;
    sockaddr_in dst_addr;
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(dport);
    if (0 == (rc = inet_aton(dst_ip.c_str(), &dst_addr.sin_addr))) {
        cerr << "Error in inet_aton while sending data.\n\tIP : " << dst_ip << endl;
        cerr << "Error Code : " << errno;
    }

    return send_data(data, sz, &dst_addr);
}

int32_t eyenet_socket::recv_data(char *buf, int32_t sz, int32_t timeout, sockaddr_in &sk_addr) {
    assert(is_bound);
    int32_t         rc = 0;
    uint32_t        sk_addr_len;
    struct pollfd   fd[1];

    fd[0].fd = sk;
    fd[0].events = POLLIN;

    int32_t pval = poll(fd, 1, timeout);
    if (pval == 1) {
        if (fd[0].revents & POLLIN) {
            sk_addr_len = sizeof(sk_addr);
            if (-1 == (rc = recvfrom(sk, buf, sz, 0, (struct sockaddr *)&sk_addr, &sk_addr_len))) {
                cerr << "Error while receiving packet. Error Code : " << errno << endl;
            }
            buf[rc] = '\0';
            return rc;
        } else {
            cerr << "Error while polling. Event Field:" << fd[0].revents << "|Error Code:" << errno << endl;
        }
    } else if (pval == 0) {
        return 0;
    }

    cerr << "Error while polling. Error Code : " << errno << endl;
    return -1;
}

void eyenet_socket::decode_sk_addr(sockaddr_in addr, string &ip, int32_t &port) {
    ip = inet_ntoa(addr.sin_addr);
    port = ntohs(addr.sin_port);
}

string eyenet_socket::decode_sk_addr(sockaddr_in addr) {
    string      ip;
    int32_t     port;
    decode_sk_addr(addr, ip, port);
    return ip + ":" + std::to_string(port);
}

#endif
