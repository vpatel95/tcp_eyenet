#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "ttest.h"

static tsnd_t      ttsnd0, *ttsnd = &ttsnd0;

static void print_conn (sworker_t *w) {

    char                local[INET_ADDRSTRLEN], remote[INET_ADDRSTRLEN];
    struct sockaddr_in  localaddr;
    socklen_t           len = sizeof(localaddr);

    if (0 != getsockname(w->fd, (struct sockaddr *)&localaddr, &len)) {
        localaddr.sin_addr.s_addr = 0;
        localaddr.sin_port = 0;
    }

    inet_ntop(AF_INET, &ttsnd->remoteip, remote, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &localaddr.sin_addr, local, INET_ADDRSTRLEN);

    printf("[%-3hu]\tlocal\t[%s:%d]\tremote\t[%s:%d]\n",
            w->id, local, ntohs(localaddr.sin_port), remote,
            ttsnd->remoteport);
}

static void print_all_conn_stats (bool delta_rep) {

    uint64_t    spkts = 0UL, sbytes = 0UL, kbps;
    uint32_t    i;
    uint32_t    intv;
   
    intv = (delta_rep) ? ttsnd->reporting_interval : ttsnd->duration;

    if (!delta_rep) {
        printf("------------------------------------------\n");
    }
    for (i = 0; i < ttsnd->n_cpus; i++) {
        sworker_t   *w = &ttsnd->workers[i];
        uint64_t    npkts, nbytes;
        sstats_t    now, prev;

        prev = w->stats_prev;
        now = w->stats;
        w->stats_prev = w->stats;
        
        npkts = (delta_rep) ? (now.npkts_tx - prev.npkts_tx) : now.npkts_tx;
        nbytes = (delta_rep) ? (now.nbytes_tx - prev.nbytes_tx) : now.nbytes_tx;
        kbps = (nbytes * 8UL) / ((uint64_t)intv * 1000UL);


        sbytes += nbytes;
        spkts += npkts;

        printf("[%-3hu]\t pkts %-10lu\t bytes %-10lu\t rate %-5lu Kbps\n",
            w->id, npkts, nbytes, kbps);
    }

    kbps = (sbytes * 8UL) / ((uint64_t)intv * 1000UL);

    printf("[SUM]\t pkts %-10lu\t bytes %-10lu\t rate %-5lu Kbps\n",
            spkts, sbytes, kbps);
}

static inline void wait_until (uint64_t till)
{           
    uint64_t n = get_time_usec();
    if (n < till) {
        uint64_t d = till - n;
        if (d < 100UL) {
            while (n < till) {
                n = get_time_usec();
            }
            return;
        }
        usleep(till - n);
    }
}

static int32_t send_pkt (int fd, struct sockaddr_in *r, const void *pkt,
                size_t plen) {

    int         flags = MSG_NOSIGNAL;
    socklen_t   alen = (socklen_t)sizeof(struct sockaddr_in);
    ssize_t     rc;

    rc = sendto(fd, pkt, plen, flags, (const struct sockaddr *)r, alen);
    if (rc == (ssize_t)plen)
        return 0;
    return -1;
}

static int32_t allocate_pkt (sworker_t *worker) {

    worker->phdr = (pkthdr_t*)malloc(sizeof(uint8_t)* worker->psize);
    if (worker->phdr) {
        worker->phdr->sig = htonl(TTEST_PKT_SIG);
        worker->phdr->pktsize = htons(worker->psize);
        worker->phdr->zero[0] = 0;
        worker->phdr->zero[1] = 0;
        worker->phdr->zero[2] = 0;
        worker->seq = 0;
        return 0;
    }
    return -1;
}

static void * snd_worker (void *arg) {

    sworker_t           *worker = &ttsnd->workers[*(uint32_t *)arg];
    pkthdr_t            *phdr = (pkthdr_t *)worker->phdr;
    struct sockaddr_in  skaddr, dst_skaddr;
    int                 fd;

    thread_set_highprio(95);

    skaddr.sin_family = AF_INET;
    ((struct in_addr*)(&skaddr.sin_addr))->s_addr = 0;
    skaddr.sin_port = 0;

    if (1 != open_data_sockets(&skaddr, &worker->fd, 1, 0)) {
        printf("failed to open recv socket\n");
        fflush(stdout);
        worker->wstatus = 2;
        return NULL;
    }
    fd = worker->fd;

    dst_skaddr.sin_family = AF_INET;
    ((struct in_addr*)(&dst_skaddr.sin_addr))->s_addr = ttsnd->remoteip.s_addr;
    dst_skaddr.sin_port = htons(ttsnd->remoteport);

    worker->npkts_burst = ttsnd->npkts_burst;
    worker->qouta = ttsnd->npkts_burst;
    worker->ibg = ttsnd->ibg * 1000UL;
    worker->seq = 0;

    if (0 != allocate_pkt(worker)) {
        printf("failed to allocate pkt for worker %d\n", worker->id);
        fflush(stdout);
        worker->wstatus = 3;
        return NULL;
    }
    
    phdr = worker->phdr;
    worker->wstatus = 1;

    while (1 != *((volatile uint32_t *)&ttsnd->cmd)) {
        usleep(100);
    }

    while (1 == *((volatile uint32_t *)&ttsnd->cmd)) {
        /*                  
         * 1. receieve data from the socket.                                   
         * 2. ensure that its a valid ttest pkt                                
         * 3. get the client conn from the addr
         * 4. incr the stats of the client conn
         */
        uint64_t            s = get_time_usec(), delta;
        uint32_t            i, seq = worker->seq;

        for (i = 0; i < worker->qouta; i++) {
            if(0 != send_pkt(fd, &dst_skaddr, phdr, worker->psize)) {
                continue;
            }
            seq++;
            phdr->seq = htonl(seq);
        }
        worker->stats.nbytes_tx += (worker->psize * (seq - worker->seq));
        worker->stats.npkts_tx += (seq - worker->seq);
        worker->seq = seq;
        delta = get_time_usec() - s;
        if (delta >= worker->ibg) {
            worker->qouta = (worker->npkts_burst * delta) / worker->ibg;
            continue;
        }
        worker->qouta = worker->npkts_burst;
        wait_until(s + worker->ibg);
    }

    worker->wstatus = 100;
    return NULL;
}

static void calc_pkts_rate (uint64_t rate_kbps, uint64_t psize,
        uint64_t *ibg_ms, uint64_t *npkts_ibg) {

#define DEF_IBG_MSEC    50UL
    uint64_t    ibg = DEF_IBG_MSEC;
    uint64_t    np_sec = (rate_kbps * 1000UL)/ (psize * 8UL);
    uint64_t    np_ibg = (np_sec * ibg)/ 1000UL;
    if (np_ibg == 0UL) {
        ibg = 1000UL / np_sec;
        np_ibg = 1UL;
    }
    *npkts_ibg = np_ibg;
    *ibg_ms = ibg;
    printf("TargetRate %lu Kbps | IBG %lu millisecs | Npkts@IBG %lu | PktSize %lu\n",
            rate_kbps, *ibg_ms, *npkts_ibg, psize);
    fflush(stdout);
}

int32_t start_client (prog_param_t *p) {

    uint32_t    i;
    uint64_t    delta, s, duration;

    ttsnd->n_cpus = p->n_cpus;
    ttsnd->remoteip = p->ip.remote;
    ttsnd->remoteport = p->port.remote;
    ttsnd->reporting_interval = p->reporting_interval;
    ttsnd->duration = p->duration;

    calc_pkts_rate((uint64_t)p->rate_kbps, (uint64_t)p->pktsize,
            &ttsnd->ibg, &ttsnd->npkts_burst);

    ttsnd->workers = (sworker_t *)malloc(sizeof(sworker_t) * p->n_cpus);
    if (!ttsnd->workers) {
        printf("failed to allocate mem for workers\n");
        return -1;
    }
    memset(ttsnd->workers, 0, sizeof(sworker_t) * p->n_cpus);

    ttsnd->cmd = 0;

    for (i = 0; i < p->n_cpus; i++) {
        sworker_t   *w = &ttsnd->workers[i];

        w->wstatus = 0;
        w->fd = -1;
        w->psize = p->pktsize;
        w->id = i;

        if (0 != pthread_create(&w->tid, NULL, snd_worker, &i)) {
            printf("failed to create worker pthread\n");
            return -1;
        }

        while (0 == *(volatile uint32_t*)&w->wstatus) {
            usleep(100);
        }

        if (w->wstatus != 1) {
            printf("failed to initialize worker pthread. wstatus %d\n",
                    w->wstatus);
            return -1;
        }

        print_conn(w);
    }

    ttsnd->cmd = 1;

    s = get_time_usec();
    duration = ttsnd->duration * 1000000UL;
    delta = 0UL;
    while (delta < duration) {
        usleep(p->reporting_interval * 1000000);
        print_all_conn_stats(true);
        delta = get_time_usec() - s;
    }

    ttsnd->cmd = 2;

    print_all_conn_stats(false);
    for (i = 0; i < p->n_cpus; i++) {
        sworker_t   *w = &ttsnd->workers[i];
        (void)pthread_join(w->tid, NULL);
    }

    return 0;
}
