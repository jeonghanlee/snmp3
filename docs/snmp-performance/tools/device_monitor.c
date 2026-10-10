/* Read-only tiered monitoring schedule: three OID tiers with their own periods, grouped K OIDs per
 * PDU, one request in flight, strict priority state > measurement > slow between PDUs.
 * GET only; credentials are read from files and never printed.
 * Stops (exit 10) on an SNMP error status or when two or more PDUs are lost and they exceed 0.5 percent
 * of all PDUs sent so far (checked after the first 40 PDUs). */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/large_fd_set.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TIERS 3
#define MAXOID 512
#define MAXCYCLE 4096
static const char *tier_name[TIERS] = {"state", "measurement", "slow"};

struct tier {
    oid oids[MAXOID][MAX_OID_LEN];
    size_t lens[MAXOID], count;
    double period_ms, next_due;
    unsigned long pdus_per_cycle;
    unsigned long next_pdu;       /* next PDU of the active cycle to send */
    int active;
    double cycle_start;
    unsigned long cycle_lost;
    double cycles[MAXCYCLE];
    unsigned long ncycles, overruns, lost_pdus, sent_pdus, bad_cycles;
};

static struct tier tiers[TIERS];
static void *handle;
static unsigned long group_k;
static int in_flight_tier = -1;
static double sent_at, pdu_latency[200000];
static unsigned long npdu_latency, total_sent, total_lost, total_err;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static size_t slurp(const char *path, unsigned char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) { fprintf(stderr, "cannot open secret file\n"); exit(2); }
    n = fread(buf, 1, cap, f);
    fclose(f);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) --n;
    return n;
}

static int cmp(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void load_tier(struct tier *t, const char *path, double period_ms)
{
    char line[512];
    FILE *file = fopen(path, "r");
    if (!file) exit(2);
    while (t->count < MAXOID && fgets(line, sizeof(line), file)) {
        char *cursor = line;
        size_t arcs = 0;
        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;
        if (*cursor == '.') ++cursor;
        while (*cursor && arcs < MAX_OID_LEN) {
            char *end;
            t->oids[t->count][arcs++] = strtoul(cursor, &end, 10);
            cursor = end;
            if (*cursor == '.') ++cursor;
        }
        t->lens[t->count++] = arcs;
    }
    fclose(file);
    t->period_ms = period_ms;
    t->pdus_per_cycle = (t->count + group_k - 1) / group_k;
}

static int callback(int op, netsnmp_session *s, int reqid, netsnmp_pdu *pdu, void *magic)
{
    struct tier *t = &tiers[(long)(intptr_t)magic];
    (void)s; (void)reqid;
    if (op == NETSNMP_CALLBACK_OP_RESEND || op == NETSNMP_CALLBACK_OP_CONNECT) return 1;
    pdu_latency[npdu_latency < 200000 ? npdu_latency++ : 199999] = now_ms() - sent_at;
    if (!(op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu && pdu->errstat == SNMP_ERR_NOERROR)) {
        ++t->lost_pdus; ++t->cycle_lost; ++total_lost;
        if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu) ++total_err;
    }
    in_flight_tier = -1;
    return 1;
}

static void send_next(int index, double now)
{
    struct tier *t = &tiers[index];
    netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET);
    size_t first = t->next_pdu * group_k, k;
    for (k = 0; k < group_k && first + k < t->count; ++k)
        snmp_add_null_var(p, t->oids[first + k], t->lens[first + k]);
    ++t->next_pdu;
    ++t->sent_pdus; ++total_sent;
    sent_at = now;
    in_flight_tier = index;
    if (!snmp_sess_async_send(handle, p, callback, (void *)(intptr_t)index)) {
        snmp_free_pdu(p);
        ++t->lost_pdus; ++t->cycle_lost; ++total_lost;
        in_flight_tier = -1;
    }
}

static void print_tier(int i)
{
    struct tier *t = &tiers[i];
    double sorted[MAXCYCLE];
    unsigned long n = t->ncycles;
    memcpy(sorted, t->cycles, n * sizeof(double));
    qsort(sorted, n ? n : 1, sizeof(double), cmp);
    printf("{\"tier\":\"%s\",\"oids\":%zu,\"pdus_per_cycle\":%lu,\"period_ms\":%.0f,\"cycles\":%lu,"
           "\"overruns\":%lu,\"cycle_p50_ms\":%.1f,\"cycle_p95_ms\":%.1f,\"cycle_max_ms\":%.1f,"
           "\"cycles_with_loss\":%lu,\"pdus_sent\":%lu,\"pdus_lost\":%lu}\n",
           tier_name[i], t->count, t->pdus_per_cycle, t->period_ms, n, t->overruns,
           n ? sorted[n / 2] : 0.0, n ? sorted[(n * 95) / 100 >= n ? n - 1 : (n * 95) / 100] : 0.0,
           n ? sorted[n - 1] : 0.0, t->bad_cycles, t->sent_pdus, t->lost_pdus);
}

int main(int argc, char **argv)
{
    /* peer user authalg privalg authfile privfile timeoutms duration_s k
       state_file state_ms meas_file meas_ms slow_file slow_ms */
    netsnmp_session sess;
    unsigned char auth[1100], priv[1100];
    size_t alen, plen;
    double begin, end_at;
    int i, stopped = 0;

    if (argc != 16) {
        fprintf(stderr, "usage: device_monitor PEER USER AUTH PRIV AUTHFILE PRIVFILE TIMEOUTMS DURATION_S K "
                        "STATEFILE STATEMS MEASFILE MEASMS SLOWFILE SLOWMS\n");
        return 2;
    }
    alen = slurp(argv[5], auth, sizeof(auth));
    plen = slurp(argv[6], priv, sizeof(priv));
    group_k = strtoul(argv[9], NULL, 10);
    if (group_k < 1 || group_k > 128) return 2;
    load_tier(&tiers[0], argv[10], strtod(argv[11], NULL));
    load_tier(&tiers[1], argv[12], strtod(argv[13], NULL));
    load_tier(&tiers[2], argv[14], strtod(argv[15], NULL));
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);
    init_snmp("device_monitor");
    snmp_sess_init(&sess);
    sess.peername = argv[1];
    sess.version = SNMP_VERSION_3;
    sess.timeout = strtol(argv[7], NULL, 10) * 1000L;
    sess.retries = 0;
    sess.securityName = argv[2];
    sess.securityNameLen = strlen(argv[2]);
    sess.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
    sess.securityAuthProto = !strcmp(argv[3], "MD5") ? usmHMACMD5AuthProtocol : usmHMACSHA1AuthProtocol;
    sess.securityAuthProtoLen = !strcmp(argv[3], "MD5") ? USM_AUTH_PROTO_MD5_LEN : USM_AUTH_PROTO_SHA_LEN;
    sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
    if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, auth, alen,
                    sess.securityAuthKey, &sess.securityAuthKeyLen) != SNMPERR_SUCCESS) return 3;
    sess.securityPrivProto = !strcmp(argv[4], "DES") ? usmDESPrivProtocol : usmAESPrivProtocol;
    sess.securityPrivProtoLen = !strcmp(argv[4], "DES") ? USM_PRIV_PROTO_DES_LEN : USM_PRIV_PROTO_AES_LEN;
    sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
    if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, priv, plen,
                    sess.securityPrivKey, &sess.securityPrivKeyLen) != SNMPERR_SUCCESS) return 3;
    handle = snmp_sess_open(&sess);
    if (!handle) { snmp_perror("snmp_sess_open"); return 4; }
    begin = now_ms();
    end_at = begin + strtod(argv[8], NULL) * 1000.0;
    for (i = 0; i < TIERS; ++i) tiers[i].next_due = begin;
    while (now_ms() < end_at && !stopped) {
        double now = now_ms(), wait_ms = 100;
        netsnmp_large_fd_set fds;
        int count = 0, block = 1, ready;
        struct timeval tv;
        /* Start cycles that are due. */
        for (i = 0; i < TIERS; ++i) {
            struct tier *t = &tiers[i];
            while (now >= t->next_due) {
                if (t->active) { ++t->overruns; }
                else { t->active = 1; t->cycle_start = t->next_due; t->next_pdu = 0; t->cycle_lost = 0; }
                t->next_due += t->period_ms;
            }
        }
        /* Dispatch the highest-priority pending PDU when nothing is in flight. */
        if (in_flight_tier < 0) {
            for (i = 0; i < TIERS; ++i) {
                struct tier *t = &tiers[i];
                if (t->active && t->next_pdu < t->pdus_per_cycle) { send_next(i, now); break; }
            }
        }
        /* A cycle is complete when all its PDUs were sent and none is in flight for it. */
        for (i = 0; i < TIERS; ++i) {
            struct tier *t = &tiers[i];
            if (t->active && t->next_pdu >= t->pdus_per_cycle && in_flight_tier != i) {
                if (t->ncycles < MAXCYCLE) t->cycles[t->ncycles++] = now_ms() - t->cycle_start;
                if (t->cycle_lost) ++t->bad_cycles;
                t->active = 0;
            }
        }
        if (total_err || (total_sent >= 40 && total_lost >= 2 && total_lost * 1000 > total_sent * 5)) stopped = 1;
        for (i = 0; i < TIERS; ++i)
            if (tiers[i].next_due - now < wait_ms) wait_ms = tiers[i].next_due - now;
        if (wait_ms < 1) wait_ms = 1;
        tv.tv_sec = (long)(wait_ms / 1000);
        tv.tv_usec = (long)((wait_ms - tv.tv_sec * 1000) * 1000);
        netsnmp_large_fd_set_init(&fds, FD_SETSIZE);
        NETSNMP_LARGE_FD_ZERO(&fds);
        snmp_sess_select_info2(handle, &count, &fds, &tv, &block);
        ready = netsnmp_large_fd_set_select(count, &fds, NULL, NULL, &tv);
        if (ready > 0) snmp_sess_read2(handle, &fds);
        else snmp_sess_timeout(handle);
        netsnmp_large_fd_set_cleanup(&fds);
    }
    for (i = 0; i < TIERS; ++i) print_tier(i);
    {
        double sorted[200000];
        unsigned long n = npdu_latency;
        memcpy(sorted, pdu_latency, n * sizeof(double));
        qsort(sorted, n ? n : 1, sizeof(double), cmp);
        printf("{\"summary\":true,\"duration_s\":%.1f,\"pdus_sent\":%lu,\"pdus_lost\":%lu,\"error_status\":%lu,"
               "\"pdu_p50_ms\":%.1f,\"pdu_p95_ms\":%.1f,\"pdu_max_ms\":%.1f,\"stopped\":%d,\"k\":%lu}\n",
               (now_ms() - begin) / 1000.0, total_sent, total_lost, total_err,
               n ? sorted[n / 2] : 0.0, n ? sorted[(n * 95) / 100 >= n ? n - 1 : (n * 95) / 100] : 0.0,
               n ? sorted[n - 1] : 0.0, stopped, group_k);
    }
    snmp_sess_close(handle);
    return stopped ? 10 : 0;
}
