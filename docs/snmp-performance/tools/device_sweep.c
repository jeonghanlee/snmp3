/* Read-only GET timing sweep: varbinds per PDU (K) and requests in flight (W) over an OID file,
 * plus an idle-latency probe mode. GET only; credentials are read from files, never printed.
 * Stops with exit code 10 when at least two PDUs and more than 0.5 percent of a pass are lost or an SNMP error
 * status (for example tooBig) is returned. */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/large_fd_set.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAXOID 1024
static oid oids[MAXOID][MAX_OID_LEN];
static size_t lens[MAXOID], oid_count;
static double sent_at[MAXOID], latency[MAXOID];
static int outcome[MAXOID];
static long errstat_seen;
static unsigned long next_index, outstanding, finished_count, total_pdus, per_pdu;
static void *handle;

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

static int callback(int op, netsnmp_session *s, int reqid, netsnmp_pdu *pdu, void *magic)
{
    unsigned long index = (unsigned long)(uintptr_t)magic;
    (void)s; (void)reqid;
    if (op == NETSNMP_CALLBACK_OP_RESEND || op == NETSNMP_CALLBACK_OP_CONNECT) return 1;
    latency[index] = now_ms() - sent_at[index];
    outcome[index] = -1;
    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu) {
        if (pdu->errstat == SNMP_ERR_NOERROR) {
            netsnmp_variable_list *v;
            outcome[index] = 1;
            for (v = pdu->variables; v; v = v->next_variable)
                if (v->type == SNMP_NOSUCHOBJECT || v->type == SNMP_NOSUCHINSTANCE || v->type == SNMP_ENDOFMIBVIEW)
                    outcome[index] = 2;
        } else { outcome[index] = -3; errstat_seen = pdu->errstat; }
    }
    --outstanding;
    ++finished_count;
    return 1;
}

static void send_pdu(unsigned long index)
{
    netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET);
    unsigned long k, first = index * per_pdu;
    for (k = 0; k < per_pdu && first + k < oid_count; ++k)
        snmp_add_null_var(p, oids[first + k], lens[first + k]);
    sent_at[index] = now_ms();
    if (!snmp_sess_async_send(handle, p, callback, (void *)(uintptr_t)index)) {
        snmp_free_pdu(p);
        latency[index] = 0;
        outcome[index] = -2;
        ++finished_count;
        return;
    }
    ++outstanding;
}

/* Returns 1 when a stop rule fired. */
static int pass(unsigned long window, unsigned long k)
{
    double start, total, sorted[MAXOID];
    unsigned long i, ok = 0, exc = 0, lost = 0, errs = 0;
    per_pdu = k;
    total_pdus = (oid_count + k - 1) / k;
    memset(outcome, 0, sizeof(outcome));
    next_index = outstanding = finished_count = 0;
    errstat_seen = 0;
    start = now_ms();
    while (finished_count < total_pdus) {
        netsnmp_large_fd_set fds;
        int count = 0, block = 1, ready;
        struct timeval tv = {1, 0};
        while (next_index < total_pdus && outstanding < window) send_pdu(next_index++);
        if (finished_count >= total_pdus) break;
        netsnmp_large_fd_set_init(&fds, FD_SETSIZE);
        NETSNMP_LARGE_FD_ZERO(&fds);
        snmp_sess_select_info2(handle, &count, &fds, &tv, &block);
        ready = netsnmp_large_fd_set_select(count, &fds, NULL, NULL, block ? NULL : &tv);
        if (ready > 0) snmp_sess_read2(handle, &fds);
        else snmp_sess_timeout(handle);
        netsnmp_large_fd_set_cleanup(&fds);
    }
    total = now_ms() - start;
    for (i = 0; i < total_pdus; ++i) {
        sorted[i] = latency[i];
        if (outcome[i] == 1) ++ok; else if (outcome[i] == 2) ++exc; else if (outcome[i] == -3) ++errs; else ++lost;
    }
    qsort(sorted, total_pdus, sizeof(double), cmp);
    printf("{\"window\":%lu,\"k\":%lu,\"pdus\":%lu,\"oids\":%zu,\"total_ms\":%.1f,\"per_oid_ms\":%.3f,"
           "\"min_ms\":%.1f,\"p50_ms\":%.1f,\"p95_ms\":%.1f,\"max_ms\":%.1f,\"ok\":%lu,\"exception\":%lu,"
           "\"lost\":%lu,\"error_status\":%lu,\"last_errstat\":%ld}\n",
           window, k, total_pdus, oid_count, total, total / oid_count, sorted[0], sorted[total_pdus / 2],
           sorted[(total_pdus * 95) / 100 >= total_pdus ? total_pdus - 1 : (total_pdus * 95) / 100],
           sorted[total_pdus - 1], ok, exc, lost, errs, errstat_seen);
    fflush(stdout);
    if (errs || (lost >= 2 && lost * 1000 > total_pdus * 5)) {
        printf("{\"stop\":\"%s\"}\n", errs ? "error-status" : "loss");
        fflush(stdout);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    /* mode peer user authalg privalg authfile privfile oidfile timeoutms repeats combos */
    netsnmp_session sess;
    unsigned char auth[1100], priv[1100];
    size_t alen, plen;
    char line[512];
    FILE *file;
    unsigned long repeats, r, i;

    if (argc != 12) {
        fprintf(stderr, "usage: device_sweep MODE PEER USER AUTH PRIV AUTHFILE PRIVFILE OIDFILE TIMEOUTMS REPEATS COMBOS\n");
        return 2;
    }
    alen = slurp(argv[6], auth, sizeof(auth));
    plen = slurp(argv[7], priv, sizeof(priv));
    file = fopen(argv[8], "r");
    if (!file) return 2;
    while (oid_count < MAXOID && fgets(line, sizeof(line), file)) {
        char *cursor = line;
        size_t arcs = 0;
        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;
        if (*cursor == '.') ++cursor;
        while (*cursor && arcs < MAX_OID_LEN) {
            char *end;
            oids[oid_count][arcs++] = strtoul(cursor, &end, 10);
            cursor = end;
            if (*cursor == '.') ++cursor;
        }
        lens[oid_count++] = arcs;
    }
    fclose(file);
    if (!oid_count) return 2;
    repeats = strtoul(argv[10], NULL, 10);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);
    init_snmp("device_sweep");
    snmp_sess_init(&sess);
    sess.peername = argv[2];
    sess.version = SNMP_VERSION_3;
    sess.timeout = strtol(argv[9], NULL, 10) * 1000L;
    sess.retries = 0;
    sess.securityName = argv[3];
    sess.securityNameLen = strlen(argv[3]);
    sess.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
    sess.securityAuthProto = !strcmp(argv[4], "MD5") ? usmHMACMD5AuthProtocol : usmHMACSHA1AuthProtocol;
    sess.securityAuthProtoLen = !strcmp(argv[4], "MD5") ? USM_AUTH_PROTO_MD5_LEN : USM_AUTH_PROTO_SHA_LEN;
    sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
    if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, auth, alen,
                    sess.securityAuthKey, &sess.securityAuthKeyLen) != SNMPERR_SUCCESS) return 3;
    sess.securityPrivProto = !strcmp(argv[5], "DES") ? usmDESPrivProtocol : usmAESPrivProtocol;
    sess.securityPrivProtoLen = !strcmp(argv[5], "DES") ? USM_PRIV_PROTO_DES_LEN : USM_PRIV_PROTO_AES_LEN;
    sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
    if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, priv, plen,
                    sess.securityPrivKey, &sess.securityPrivKeyLen) != SNMPERR_SUCCESS) return 3;
    handle = snmp_sess_open(&sess);
    if (!handle) { snmp_perror("snmp_sess_open"); return 4; }
    if (!strcmp(argv[1], "probefor")) {
        /* COMBOS is a duration in seconds: one GET of the first OID per second until it elapses. */
        double sorted[MAXOID], begin = now_ms(), limit = strtod(argv[11], NULL) * 1000.0;
        unsigned long n = 0, stops = 0;
        oid_count = 1;
        while (now_ms() - begin < limit && n < MAXOID) {
            if (pass(1, 1)) ++stops;
            sorted[n++] = latency[0];
            sleep(1);
        }
        qsort(sorted, n, sizeof(double), cmp);
        printf("{\"probefor\":%lu,\"p50_ms\":%.1f,\"p95_ms\":%.1f,\"max_ms\":%.1f,\"stops\":%lu}\n", n,
               sorted[n / 2], sorted[(n * 95) / 100 >= n ? n - 1 : (n * 95) / 100], sorted[n - 1], stops);
        snmp_sess_close(handle);
        return 0;
    }
    if (!strcmp(argv[1], "probe")) {
        /* COMBOS is the number of probes, one per second, each the first OID of the file. */
        unsigned long n = strtoul(argv[11], NULL, 10);
        double sorted[MAXOID];
        unsigned long lostc = 0;
        oid_count = 1;
        for (i = 0; i < n && i < MAXOID; ++i) {
            if (pass(1, 1)) ++lostc;
            sorted[i] = latency[0];
            sleep(1);
        }
        qsort(sorted, n, sizeof(double), cmp);
        printf("{\"probe\":%lu,\"p50_ms\":%.1f,\"p95_ms\":%.1f,\"max_ms\":%.1f,\"stops\":%lu}\n", n,
               sorted[n / 2], sorted[(n * 95) / 100 >= n ? n - 1 : (n * 95) / 100], sorted[n - 1], lostc);
        snmp_sess_close(handle);
        return 0;
    }
    /* Warm-up: one request for discovery and time synchronization, not reported. */
    { size_t keep = oid_count; oid_count = 1; if (pass(1, 1)) return 10; oid_count = keep; }
    sleep(2);
    for (r = 0; r < repeats; ++r) {
        char *copy = strdup(argv[11]), *tok;
        for (tok = strtok(copy, ","); tok; tok = strtok(NULL, ",")) {
            unsigned long k = strtoul(tok, NULL, 10), w = 1;
            char *x = strchr(tok, 'x');
            if (x) w = strtoul(x + 1, NULL, 10);
            if (k < 1 || k > 128 || w < 1 || w > 32) continue;
            printf("{\"repeat\":%lu}\n", r);
            if (pass(w, k)) { free(copy); snmp_sess_close(handle); return 10; }
            sleep(3);
        }
        free(copy);
    }
    snmp_sess_close(handle);
    return 0;
}
