#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "ttest.h"

static trcv_t     ttrcv0, *ttrcv = &ttrcv0;

static void print_conn (rconn_t *c) {

    char            local[INET_ADDRSTRLEN], remote[INET_ADDRSTRLEN];        

    inet_ntop(AF_INET, &ttrcv->localip, local, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &c->t.ip, remote, INET_ADDRSTRLEN);

    printf("[%-3lu]\tlocal\t[%s:%d]\tremote\t[%s:%d]\n",
            c->cid, local, ttrcv->localport, remote, c->t.port);
}

static void print_all_conn_stats (uint32_t intv) {

    rconn_t             *conn;
    uint64_t            sbytes = 0UL, spkts = 0UL;
    uint64_t            kbps;

    pthread_rwlock_rdlock(&ttrcv->conn_lock);
    cds_list_for_each_entry(conn, &ttrcv->lhead_conn, lelem_global) {
                
        uint64_t            npkts, nbytes;
        rstats_t            now, prev;

        prev = conn->stats_prev;
        now = conn->stats;
        conn->stats_prev = conn->stats;
        
        npkts = now.npkts_rx - prev.npkts_rx;
        nbytes = now.nbytes_rx - prev.nbytes_rx;
        kbps = (nbytes * 8UL) / ((uint64_t)intv * 1000UL);


        sbytes += nbytes;
        spkts += npkts;

        printf("[%-3lu]\t pkts %-10lu\t bytes %-10lu\t rate %-5lu Kbps\n",
            conn->cid, npkts, nbytes, kbps);

    }
    pthread_rwlock_unlock(&ttrcv->conn_lock);

    kbps = (sbytes * 8UL) / ((uint64_t)intv * 1000UL);

    if (ttrcv->n_conns) {
        printf("[SUM]\t pkts %-10lu\t bytes %-10lu\t rate %-5lu Kbps\n",
            spkts, sbytes, kbps);
        printf("------------------------------------------\n");
    }
}

static void register_rconn (rconn_t *conn) {

    pthread_rwlock_wrlock(&ttrcv->conn_lock);
    conn->cid = ttrcv->cid_seed++;
    cds_list_add_tail(&conn->lelem_global, &ttrcv->lhead_conn);
    ttrcv->n_conns++;
    pthread_rwlock_unlock(&ttrcv->conn_lock);
    print_conn(conn);
}

static rconn_t * lookup_rconn (rworker_t *worker, rconn_tuple_t *t) {

    struct cds_list_head    *conn_list;
    rconn_t                 *conn = NULL, *tconn;

    conn_list = &worker->rconn_hash[t->port & (MAX_PORTS - 1)]
                    [t->ip.s_addr & (MAX_ADDR_HASH_SZ - 1)];

    cds_list_for_each_entry(tconn, conn_list, lelem_hash) {
        if ((tconn->t.ip.s_addr == t->ip.s_addr) &&
                (tconn->t.port == t->port)) {
            conn = tconn;
            break;
        }
    }

    if (conn) return conn;

    conn = (rconn_t *)malloc(sizeof(rconn_t));
    if (conn) {
        memset(conn, 0, sizeof(rconn_t));
        conn->t = *t;
        conn->wid = worker->id;
        register_rconn(conn);
        cds_list_add(&conn->lelem_hash, conn_list);
    }
    return conn;
}

void * rcv_worker (void *arg) {

    rworker_t           *worker = &ttrcv->workers[*(uint32_t *)arg];
    pkthdr_t            phdr0, *phdr = &phdr0;
    rconn_tuple_t       t0, *t = &t0;
    struct sockaddr_in  skaddr, from;
    size_t              hdrsz = sizeof(pkthdr_t);
    socklen_t           alen = (socklen_t)sizeof(struct sockaddr_in);      
    uint32_t            i, j;
    int                 fd;

    thread_set_highprio(95);

    skaddr.sin_family = AF_INET;
    skaddr.sin_addr = ttrcv->localip;
    skaddr.sin_port = htons(ttrcv->localport);

    if (1 != open_data_sockets(&skaddr, &worker->fd, 1, 0)) {
        printf("failed to open recv socket\n");
        worker->wstatus = 2;
        return NULL;
    }
    fd = worker->fd;
    
    for (i = 0; i < MAX_PORTS; i++) {
        for (j = 0; j < MAX_ADDR_HASH_SZ; j++) {
            CDS_INIT_LIST_HEAD(&worker->rconn_hash[i][j]);
        }
    }

    worker->wstatus = 1;

    while (1 != *((volatile uint32_t *)&ttrcv->cmd)) {
        usleep(100);
    }

    while (1 == *((volatile uint32_t *)&ttrcv->cmd)) {
        /*                  
         * 1. receieve data from the socket.                                   
         * 2. ensure that its a valid ttest pkt                                
         * 3. get the client conn from the addr
         * 4. incr the stats of the client conn
         */
        ssize_t             rx_len;
        rconn_t             *conn;

        alen = (socklen_t)sizeof(struct sockaddr_in);      
        rx_len = recvfrom(fd, (void *)phdr, hdrsz, 0,
                (struct sockaddr *)&from, &alen); 
        if ((rx_len == (ssize_t)hdrsz) && (ntohl(phdr->sig) ==
                    TTEST_PKT_SIG)) {
            t->ip = from.sin_addr;
            t->port = from.sin_port;
            conn = lookup_rconn(worker, t);
            if (conn) {
                conn->stats.nbytes_rx += ntohs(phdr->pktsize);
                conn->stats.npkts_rx += 1;
                assert(conn->wid == worker->id);
            }
        }
    }

    worker->wstatus = 100;
    return NULL;
}

int32_t start_server (prog_param_t *p) {

    uint32_t    i;

    ttrcv->n_cpus = p->n_cpus;
    ttrcv->localip = p->ip.local;
    ttrcv->localport = p->port.local;
    CDS_INIT_LIST_HEAD(&ttrcv->lhead_conn);
    pthread_rwlock_init(&ttrcv->conn_lock, NULL);

    ttrcv->workers = (rworker_t *)malloc(sizeof(rworker_t) * p->n_cpus);
    if (!ttrcv->workers) {
        printf("failed to allocate mem for workers\n");
        return -1;
    }
    memset(ttrcv->workers, 0, sizeof(rworker_t) * p->n_cpus);

    ttrcv->cmd = 0;

    for (i = 0; i < p->n_cpus; i++) {
        rworker_t   *w = &ttrcv->workers[i];

        w->wstatus = 0;
        w->fd = -1;
        w->id = i;

        if (0 != pthread_create(&w->tid, NULL, rcv_worker, &i)) {
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
    }

    ttrcv->cmd = 1;

    while (1) {
        print_all_conn_stats(p->reporting_interval);
        usleep(p->reporting_interval * 1000000);
    }

    return 0;
}
