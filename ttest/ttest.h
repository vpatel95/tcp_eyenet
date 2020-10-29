#if !defined(__TTEST_H__)
#define __TTEST_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <urcu/list.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

/*
 * COMMON
 */ 
#define TTEST_PKT_SIG       0xabcddcba
typedef struct pkthdr {
    uint32_t            sig;
    uint32_t            seq;
    uint16_t            pktsize;
    uint8_t             zero[3];
} pkthdr_t;

typedef struct prog_param {
    uint32_t                mode; /* 0 for client, 1 for server */
    union {
        struct in_addr      local;
        struct in_addr      remote;
        struct in_addr      gen;
    }                       ip;
    union {
        uint16_t            local;
        uint16_t            remote;
        uint16_t            gen;
    }                       port;
    uint32_t                pktsize;
    uint32_t                duration;
    uint32_t                reporting_interval;
    uint32_t                rate_kbps;
    uint32_t                n_cpus;
} prog_param_t;

/*
 * Receiver
 */ 
#define MAX_PORTS           8192
#define MAX_ADDR_HASH_SZ    4

typedef struct rconn_tuple {
    struct in_addr      ip;     /* client ip */
    uint16_t            port;   /* client port */
    uint16_t            wid;
} rconn_tuple_t;

typedef struct rstats {
    uint64_t                nbytes_rx;
    uint64_t                npkts_rx;
    uint64_t                npkts_dropped;
    uint64_t                npkts_ofo;
} rstats_t;

typedef struct rconn {
    rconn_tuple_t           t;
    uint64_t                cid;
    struct cds_list_head    lelem_hash;
    struct cds_list_head    lelem_global;
    rstats_t                stats;
    rstats_t                stats_prev;
    uint16_t                wid;
} rconn_t;

typedef struct rworker {
    pthread_t               tid;
    uint32_t                wstatus;
    int                     fd;
    struct cds_list_head    rconn_hash[MAX_PORTS][MAX_ADDR_HASH_SZ];
    uint16_t                id;
} rworker_t;

typedef struct trcv {
    uint32_t                cmd;
    uint8_t                 n_cpus;
    struct in_addr          localip;
    uint16_t                localport;
    struct cds_list_head    lhead_conn;
    uint64_t                n_conns;
    uint64_t                cid_seed;
    rworker_t               *workers;
    pthread_rwlock_t        conn_lock;
} trcv_t;

/*
 * Sender 
 */ 
typedef struct sstats {
    uint64_t                nbytes_tx;
    uint64_t                npkts_tx;
} sstats_t;

typedef struct sworker {
    pthread_t               tid;
    uint32_t                wstatus;
    int                     fd;
    uint16_t                psize;
    uint16_t                id;
    uint32_t                seq;
    uint64_t                ibg;
    uint64_t                npkts_burst;
    uint64_t                qouta;
    pkthdr_t                *phdr;
    sstats_t                stats;
    sstats_t                stats_prev;
} sworker_t;

typedef struct tsnd {
    uint32_t                cmd;
    uint8_t                 n_cpus;
    struct in_addr          remoteip;
    uint16_t                remoteport;
    uint64_t                ibg;
    uint64_t                npkts_burst;
    sworker_t               *workers;
    uint32_t                reporting_interval;
    uint32_t                duration;
} tsnd_t;

int32_t open_data_sockets (struct sockaddr_in *laddr_arr, int *datafd_arr,
        int num_sks, uint32_t rcv_tmo_usec);
int32_t start_server (prog_param_t *p);
int32_t start_client (prog_param_t *p);
void thread_set_highprio (int val);

static inline uint64_t get_time_usec (void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (uint64_t)((tp.tv_sec * 1000000UL) + (tp.tv_nsec /
                1000UL));
}

#endif /* ! __TTEST_H__ */
