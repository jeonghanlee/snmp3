/* Net-SNMP single-session throughput baseline: sequential, concurrent and multi-varbind GETs. */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/library/large_fd_set.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long done, failed;

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
    if (!f) { perror(path); exit(2); }
    n = fread(buf, 1, cap, f);
    fclose(f);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) --n;
    return n;
}

static int callback(int op, netsnmp_session *s, int reqid, netsnmp_pdu *pdu, void *magic)
{
    (void)s; (void)reqid; (void)magic;
    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE && pdu && pdu->errstat == SNMP_ERR_NOERROR)
        ++done;
    else if (op == NETSNMP_CALLBACK_OP_RESEND || op == NETSNMP_CALLBACK_OP_CONNECT)
        return 1;
    else
        ++failed;
    return 1;
}

int main(int argc, char **argv)
{
    /* peer version n mode community auth priv user */
    netsnmp_session sess, *s;
    void *h;
    const char *mode, *version;
    unsigned long n, i;
    static const oid object[] = {1, 3, 6, 1, 4, 1, 53864, 4, 1, 0};
    unsigned char community[256], auth[1100], priv[1100];
    size_t clen = 0, alen = 0, plen = 0;
    double t0, t1;

    if (argc != 9) {
        fprintf(stderr, "usage: baseline PEER VERSION N MODE COMMUNITYFILE AUTHFILE PRIVFILE USER\n");
        return 2;
    }
    version = argv[2];
    n = strtoul(argv[3], NULL, 10);
    mode = argv[4];
    clen = slurp(argv[5], community, sizeof(community));
    alen = slurp(argv[6], auth, sizeof(auth));
    plen = slurp(argv[7], priv, sizeof(priv));
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_PERSIST_STATE, 1);
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_ALARM_DONT_USE_SIG, 1);
    init_snmp("baseline");
    snmp_sess_init(&sess);
    sess.peername = argv[1];
    sess.timeout = 5000000L;
    sess.retries = 0;
    if (!strcmp(version, "2c")) {
        sess.version = SNMP_VERSION_2c;
        sess.community = community;
        sess.community_len = clen;
    } else {
        sess.version = SNMP_VERSION_3;
        sess.securityName = argv[8];
        sess.securityNameLen = strlen(argv[8]);
        sess.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
        sess.securityAuthProto = usmHMACSHA1AuthProtocol;
        sess.securityAuthProtoLen = USM_AUTH_PROTO_SHA_LEN;
        sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
        if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, auth, alen,
                        sess.securityAuthKey, &sess.securityAuthKeyLen) != SNMPERR_SUCCESS) return 3;
        sess.securityPrivProto = usmAESPrivProtocol;
        sess.securityPrivProtoLen = USM_PRIV_PROTO_AES_LEN;
        sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
        if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen, priv, plen,
                        sess.securityPrivKey, &sess.securityPrivKeyLen) != SNMPERR_SUCCESS) return 3;
    }
    h = snmp_sess_open(&sess);
    if (!h) { snmp_perror("snmp_sess_open"); return 4; }
    s = snmp_sess_session(h);
    (void)s;

    /* One warm-up request so that v3 discovery and time synchronization are outside the timing. */
    {
        netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET), *r = NULL;
        snmp_add_null_var(p, object, OID_LENGTH(object));
        if (snmp_sess_synch_response(h, p, &r) != STAT_SUCCESS) { snmp_perror("warmup"); return 5; }
        snmp_free_pdu(r);
    }

    t0 = now_ms();
    if (!strcmp(mode, "seq")) {
        for (i = 0; i < n; ++i) {
            netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET), *r = NULL;
            snmp_add_null_var(p, object, OID_LENGTH(object));
            if (snmp_sess_synch_response(h, p, &r) == STAT_SUCCESS && r && r->errstat == SNMP_ERR_NOERROR) ++done;
            else ++failed;
            if (r) snmp_free_pdu(r);
        }
    } else if (!strcmp(mode, "multi")) {
        netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET), *r = NULL;
        for (i = 0; i < n; ++i) snmp_add_null_var(p, object, OID_LENGTH(object));
        if (snmp_sess_synch_response(h, p, &r) == STAT_SUCCESS && r && r->errstat == SNMP_ERR_NOERROR) done = n;
        else failed = n;
        if (r) snmp_free_pdu(r);
    } else if (!strcmp(mode, "conc")) {
        for (i = 0; i < n; ++i) {
            netsnmp_pdu *p = snmp_pdu_create(SNMP_MSG_GET);
            snmp_add_null_var(p, object, OID_LENGTH(object));
            if (!snmp_sess_async_send(h, p, callback, NULL)) { ++failed; snmp_free_pdu(p); }
        }
        while ((unsigned long)(done + failed) < n) {
            netsnmp_large_fd_set fds;
            int count = 0, block = 1, ready;
            struct timeval tv = {1, 0};
            netsnmp_large_fd_set_init(&fds, FD_SETSIZE);
            NETSNMP_LARGE_FD_ZERO(&fds);
            snmp_sess_select_info2(h, &count, &fds, &tv, &block);
            ready = netsnmp_large_fd_set_select(count, &fds, NULL, NULL, block ? NULL : &tv);
            if (ready > 0) snmp_sess_read2(h, &fds);
            else snmp_sess_timeout(h);
            netsnmp_large_fd_set_cleanup(&fds);
        }
    } else return 2;
    t1 = now_ms();
    printf("{\"tool\":\"net-snmp-direct\",\"version\":\"%s\",\"mode\":\"%s\",\"n\":%lu,"
           "\"elapsed_ms\":%.3f,\"per_request_ms\":%.3f,\"ok\":%ld,\"failed\":%ld}\n",
           version, mode, n, t1 - t0, (t1 - t0) / (double)n, done, failed);
    snmp_sess_close(h);
    return failed ? 1 : 0;
}
