#include <stdio.h>
#include <stdint.h>
#include "ttest.h"

/*
 * Internal Functions
 */ 
#define ONE_MB                      (1000000)
#define SK_BUF_SZ_RX                (100*ONE_MB)
#define SK_BUF_SZ_TX                (100*ONE_MB)

void thread_set_highprio (int val)
{
    struct sched_param  param;

    param.sched_priority = val;
    if (0 != sched_setscheduler(0, SCHED_RR, &param))
        printf("sched_setscheduler failed. %s\n", strerror(errno)); 
}

static void validate_socketbuffers (int fd, uint32_t snd_sz, uint32_t rcv_sz)
{
    uint32_t    sz;
    socklen_t   optlen = sizeof(sz);
    (void)getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, &optlen);
    if (sz < snd_sz) {
        printf("fd %d. send-buf sz %u < configured snd-buf sz %u\n",
                fd, sz, snd_sz);
    }
    optlen = sizeof(sz);
    (void)getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, &optlen);
    if (sz < rcv_sz) {
        printf("fd %d. recv-buf sz %u < configured recv-buf sz %u\n",
                fd, sz, rcv_sz);
    }
}

static void setup_socketbuffers (int fd, uint32_t snd_sz, uint32_t rcv_sz)
{
    if (snd_sz) {
        printf("fd %d: setting snd_buf sz to %d\n", fd, snd_sz);
        if (0 != setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd_sz,
                    sizeof(snd_sz))) {
            printf("fd: %d: setting snd_buf sz %u failed: %s\n",
                    fd, snd_sz, strerror(errno));
        }
    }
    if (rcv_sz) {
        printf("fd %d: setting rcv_buf sz to %d\n", fd, rcv_sz);
        if (0 != setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv_sz,
                    sizeof(rcv_sz))) {
            printf("fd: %d: setting rcv_buf sz %u failed: %s\n",
                    fd, rcv_sz, strerror(errno));
        }
    }
}

int32_t open_data_sockets (struct sockaddr_in *laddr_arr, int *datafd_arr,
        int num_sks, uint32_t rcv_tmo_usec)
{
    int                 i;
    int                 fd;
    int                 enable = 1;
    struct timeval      tv;

    for (i = 0; i < num_sks; i++) {

        struct sockaddr_in *laddr = &laddr_arr[i];

        if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            printf("failed to open data socket: %s\n",
                    strerror(errno));
            goto failed;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable,
                    sizeof(int)) < 0) {
            printf("setsockopt(SO_REUSEPORT) failed: %s", strerror(errno));
        }
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                    sizeof(int)) < 0) {
            printf("setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        }

        if (rcv_tmo_usec) {
            tv.tv_sec = 0;
            tv.tv_usec = rcv_tmo_usec;
            if (0 > setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))){
                printf("failed to set rcv tmo for data local socket: %s\n",
                        strerror(errno));
            }
        }

        setup_socketbuffers(fd, SK_BUF_SZ_RX, SK_BUF_SZ_TX);

        validate_socketbuffers(fd, SK_BUF_SZ_RX, SK_BUF_SZ_TX);

        if (laddr->sin_addr.s_addr != 0)  {
            if (0 > bind(fd, (struct sockaddr *)laddr, sizeof(*laddr))) {
                printf("failed to bind data local socket: %s\n",
                        strerror(errno));
                close(fd);
                goto failed;
            }
        }

        datafd_arr[i] = fd;
    }
    return i;

failed:
    while(i-- > 0) {
        close(datafd_arr[i]);
        datafd_arr[i] = -1;
    }
    return -1;
}

static void usage (char *prog) {
    printf("%s : UDP based Traffic Tester\n", prog);
    printf("Usage:\n\n");
    printf("Mode - Client\n");
    printf("===============\n");
    printf("\t-c <remote ip> : remote server ipaddr\n");
    printf("\t-p <remote port number> : remote server port number\n");
    printf("\t-b <num>[KM] : rate in [K|M]bps\n");
    printf("\t-P <num> : number of parallel connection (each of -b rate)\n");
    printf("\t-t <secs> : number of secs to run\n");
    printf("\t-i <secs> : reporting interval in seconds\n");
    printf("\t-l <num> : packet length\n");
    printf("\n");
    printf("Mode - Server\n");
    printf("===============\n");
    printf("\t-s <local ip> : server binding to local ip\n");
    printf("\t-p <local port number> : server binding port number\n");
    printf("\t-i <secs> : reporting interval in seconds\n");
    printf("\t-P <num> : number of parallel receiver threads\n");
    printf("\n\n");
}

static int32_t parse_args (int argc, char *argv[], prog_param_t *p) {

    uint32_t        v;
    char            c;

    /* defualts */
    p->port.gen = 5001;
    p->pktsize = 1472;
    p->duration = 5;
    p->reporting_interval = 1;
    p->rate_kbps = 10 * 1000; // 10 mbps
    p->n_cpus = 1;

    /*
     * Client Options
     * -------------- 
     * -c <remote ip> : remote server ipaddr
     * -p <remote port number> : remote server port number
     * -b <num>[KM] : rate in [K|M]bps
     * -P <num> : number of parallel connection (each of -b rate)
     * -t <secs> : number of secs to run
     * -i <secs> : reporting interval in seconds
     * -l <num> : packet length
     *
     * Server Options
     * -------------- 
     * -s <local_ip> : server binding to local ip
     * -p : port_number
     * -i <secs> : reporting interval in seconds
     * -P <num> : number of parallel receiver threads
     */
    while ((c = getopt (argc, argv, "c:s:p:b:P:t:i:l:")) != -1) {
        switch (c) {
            case 'c':
                if (p->mode != 0) {
                    printf("both client/server mode not allowed\n");
                    return -1;
                }
                p->mode = 1;
                if (1 != inet_pton(AF_INET, optarg, (void*)&p->ip.gen)) {
                    printf("illegal remote server ip %s\n", optarg);
                    return -1;
                }
                break;
            case 's':
                if (p->mode != 0) {
                    printf("both client/server mode not allowed\n");
                    return -1;
                }
                p->mode = 2;
                if (1 != inet_pton(AF_INET, optarg, (void*)&p->ip.gen)) {
                    printf("illegal local bind ip %s\n", optarg);
                    return -1;
                }
                break;
            case 'p':
                v = (uint32_t)atoi(optarg);
                if ((v == 0) || (v > 65535)) {
                    printf("improper port number %s\n", optarg);
                    return -1;
                }
                p->port.gen = (uint16_t)v;
                break;
            case 'P':
                v = (uint32_t)atoi(optarg);
                if (v > 1000) {
                    printf("more than 1000 conns/threads not allowed\n");
                    return -1;
                }
                p->n_cpus = ((v == 0) ? 1 : v);
                break;
            case 't':
                v = (uint32_t)atoi(optarg);
                if (v == 0) {
                    printf("illegal duration %s\n", optarg);
                    return -1;
                }
                p->duration = v;
                break;
            case 'i':
                v = (uint32_t)atoi(optarg);
                if (v == 0) {
                    printf("illegal interval %s secs\n", optarg);
                    return -1;
                }
                p->reporting_interval = v;
                break;
            case 'l':
#define min_pkt_sz (uint32_t)((sizeof(pkthdr_t) < 64) ? 64 : sizeof(pkthdr_t))
                v = (uint32_t)atoi(optarg);
                if ((v < min_pkt_sz) || (v > 1472)) {
                    printf("illegal pkt len %s. should be in [%u - 1472]"
                           " range\n", optarg, min_pkt_sz);
                    return -1;
                }
                p->pktsize = (uint16_t)v;
                break;
            case 'b':
                {
                    size_t      s = strlen(optarg);
                    char        u = optarg[s - 1];
                    uint32_t    rate;

                    if ((u != 'K') && (u != 'M')) {
                        printf("rate should have either 'K' or 'M' suffix\n");
                        return -1;
                    }
                    rate = (u == 'M') ? 1000 : 1;
                    optarg[s - 1] = 0;
                    v = (uint32_t)atoi(optarg);
                    optarg[s - 1] = u;
                    rate *= v;
                    if ((rate < 100) || (rate > 1000000)) {
                        printf("illegal kbps rate %u. should be in"
                               " [100 - 1000000] range\n", rate);
                        return -1;
                    }
                    p->rate_kbps = rate;
                    break;
                }
            default:
                usage(argv[0]);
                return 0;
        }
    }

    if (p->mode == 0) {
        printf("specify either -c or -s\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}

int main (int a, char *c[]) {

    prog_param_t    param0, *p = &param0;

    memset(p, 0, sizeof(prog_param_t));

    if (a == 1) {
        usage(c[0]);
        return 0;
    }

    if (0 != parse_args(a, c, p)) {
        return -1;
    }

    if (p->mode == 0) {
        return 0;
    }

    thread_set_highprio(92);

    if (p->mode == 1) {
        start_client(p);
    } else {
        start_server(p);
    }

    return 0;
}
