#include <stdio.h>
#include <lsquic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/crypto.h>

#define EV_STANDALONE 1
#define EV_API_STATIC 1
#include "ev.c"
#include "net.h"
#include "lsquic_utils.h"
#include "client.h"

static lsquic_conn_ctx_t *on_new_conn_cb (void *ea_stream_if_ctx, lsquic_conn_t *conn);
static void on_conn_closed_cb (lsquic_conn_t *conn);
static lsquic_stream_ctx_t *on_new_stream_cb (void *ea_stream_if_ctx, lsquic_stream_t *stream);
static void on_read_cb (lsquic_stream_t *stream, lsquic_stream_ctx_t *h);
static void on_write_cb (lsquic_stream_t *stream, lsquic_stream_ctx_t *h);
static void on_hsk_done (lsquic_conn_t *c, enum lsquic_hsk_status s);
void *run_ev_thread_fun(void *arg);

typedef struct State {
    // event loop
    struct ev_loop *loop;
    ev_io sock_watcher;
    ev_io stdin_watcher;
    ev_timer conn_watcher;

    // lsquic
    int sockfd;
    struct sockaddr_storage local_sas;
    struct sockaddr_in peer_addr;
     lsquic_engine_t *engine;
    lsquic_conn_t *conn;
    lsquic_stream_t *stream;

    // msg to send
    char *buf;
    int size;
} State;

static void process_conns(State *state);

const struct lsquic_stream_if stream_if = {
        .on_new_conn            = on_new_conn_cb,
        .on_conn_closed         = on_conn_closed_cb,
        .on_new_stream          = on_new_stream_cb,
        .on_read                = on_read_cb,
        .on_write               = on_write_cb,
        .on_hsk_done            = on_hsk_done
};


static int send_packets_out(void *ctx, const struct lsquic_out_spec *specs, unsigned n_specs)
{
    printf("send_packets_out\n");
    fflush(stdout);
    struct msghdr msg;
    int *sockfd;
    unsigned n;

    memset(&msg, 0, sizeof(msg));
    sockfd = (int *) ctx;

    for (n = 0; n < n_specs; ++n)
    {
        msg.msg_name       = (void *) specs[n].dest_sa;
        msg.msg_namelen    = sizeof(struct sockaddr_in);
        msg.msg_iov        = specs[n].iov;
        msg.msg_iovlen     = specs[n].iovlen;
        if (sendmsg(*sockfd, &msg, 0) < 0) {
            perror("cannot send\n");
            break;
        }
    }

    return (int) n;
}

static void read_sock(EV_P_ ev_io *w, int revents) {
    printf("read sock\n");
    fflush(stdout);
    State *state = w->data;
    ssize_t nread;
    struct sockaddr_storage peer_sas;
    unsigned char buf[0x1000];
    struct iovec vec[1] = {{ buf, sizeof(buf) }};

    struct msghdr msg = {
            .msg_name       = &peer_sas,
            .msg_namelen    = sizeof(peer_sas),
            .msg_iov        = vec,
            .msg_iovlen     = 1,
    };
    nread = recvmsg(w->fd, &msg, 0);
    if (-1 == nread) {
        return;
    }

    // TODO handle ECN properly
    int ecn = 0;

    (void) lsquic_engine_packet_in(state->engine, buf, nread,
                                   (struct sockaddr *) &state->local_sas,
                                   (struct sockaddr *) &peer_sas,
                                   (void *) (uintptr_t) w->fd, ecn);

    process_conns(state);
}

static void process_conns_cb(EV_P_ ev_timer *conn_watcher, int revents) {
    process_conns(conn_watcher->data);
}

void process_conns(State *state) {
    printf("process_conns\n");
    fflush(stdout);
    int diff;
    ev_tstamp timeout;

    ev_timer_stop(state->loop, &state->conn_watcher);
    lsquic_engine_process_conns(state->engine);
    if (lsquic_engine_earliest_adv_tick(state->engine, &diff)) {
        if (diff >= LSQUIC_DF_CLOCK_GRANULARITY)
            timeout = (ev_tstamp) diff / 1000000;
        else if (diff <= 0)
            timeout = 0.0;
        else
            timeout = (ev_tstamp) LSQUIC_DF_CLOCK_GRANULARITY / 1000000;
        ev_timer_init(&state->conn_watcher, process_conns_cb, timeout, 0.);
        ev_timer_start(state->loop, &state->conn_watcher);
    }
}

static lsquic_conn_ctx_t *on_new_conn_cb(void *ea_stream_if_ctx, lsquic_conn_t *conn) {
    printf("On new connection\n");
    State *state = ea_stream_if_ctx;
    fflush(stdout);
    return (void *) state;
}

static void on_conn_closed_cb(lsquic_conn_t *conn) {
    printf("On connection close\n");
    char errbuf[2048];
    enum LSQUIC_CONN_STATUS status = lsquic_conn_status(conn, errbuf, 2048);
    printf("errbuf: %s\n", errbuf);
    printf("conn status: %s\n", get_conn_status_str(status));
    fflush(stdout);
}

static void on_hsk_done(lsquic_conn_t *conn, enum lsquic_hsk_status status) {
    State *state = (void *) lsquic_conn_get_ctx(conn);

    switch (status)
    {
        case LSQ_HSK_OK:
        case LSQ_HSK_RESUMED_OK:
            printf("handshake successful, start stdin watcher\n");
            fflush(stdout);
            lsquic_conn_make_stream(state->conn);
            break;
        default:
            printf("handshake failed\n");
            fflush(stdout);
            break;
    }
}

static lsquic_stream_ctx_t *on_new_stream_cb(void *ea_stream_if_ctx, lsquic_stream_t *stream) {
    printf("On new stream\n");
    fflush(stdout);
    State *state = ea_stream_if_ctx;
    state->stream = stream;
    ev_io_start(state->loop, &state->stdin_watcher);
    return (void *) state;
}

static void on_read_cb(lsquic_stream_t *stream, lsquic_stream_ctx_t *h) {
    lsquic_conn_t *conn = lsquic_stream_conn(stream);
    State *state = (void *) lsquic_conn_get_ctx(conn);

    unsigned char buf[256] = {0};

    ssize_t nr = lsquic_stream_read(stream, buf, sizeof(buf));

    buf[nr] = '\0';
    printf("recv %zd bytes: %s\n", nr, buf);
    fflush(stdout);

    lsquic_stream_wantread(stream, 0);
    ev_io_start(state->loop, &state->stdin_watcher);
}

static void on_write_cb(lsquic_stream_t *stream, lsquic_stream_ctx_t *h) {
    lsquic_conn_t *conn = lsquic_stream_conn(stream);
    State *state = (void *) lsquic_conn_get_ctx(conn);

    lsquic_stream_write(stream, state->buf, state->size);
    lsquic_stream_wantwrite(stream, 0);
    lsquic_stream_flush(stream);
    lsquic_stream_wantread(stream, 1);
}

void read_stdin(EV_P_ ev_io *w, int revents) {
    State *state = w->data;
    char *lineptr = NULL;
    size_t n = 0;
    ssize_t read_bytes = getline(&lineptr, &n, stdin);
    if (read_bytes == -1) {
        printf("getline error\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    state->buf = lineptr;
    state->size = read_bytes;
    ev_io_stop(state->loop, w);
    lsquic_stream_wantwrite(state->stream, 1);
    process_conns(state);
}

void create_event_loop(State *state) {
    state->loop = EV_DEFAULT;
    state->sock_watcher.data = state;
    state->stdin_watcher.data = state;
    state->conn_watcher.data = state;
    ev_io_init (&state->sock_watcher, read_sock, state->sockfd, EV_READ);
    ev_io_start (state->loop, &state->sock_watcher);
//    ev_io_init (&state->stdin_watcher, read_stdin, 0 /* STDIN*/, EV_READ);
    ev_init(&state->conn_watcher, process_conns_cb);
}

struct ssl_ctx_st *ssl_ctx;

SSL_CTX *get_ssl_ctx(void *peer_ctx, const struct sockaddr *unused) {
    // TODO pass ssl_ctx in peer_ctx
    (void) peer_ctx;
    (void) unused;
    return ssl_ctx;
}

static int init_ssl_ctx ()
{
    unsigned char ticket_keys[48];

    ssl_ctx = SSL_CTX_new(TLS_method());
//    if (!prog->prog_ssl_ctx)
//    {
//        LSQ_ERROR("cannot allocate SSL context");
//        return -1;
//    }

    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_default_verify_paths(ssl_ctx);

    /* This is obviously test code: the key is just an array of NUL bytes */
    memset(ticket_keys, 0, sizeof(ticket_keys));
    if (1 != SSL_CTX_set_tlsext_ticket_keys(ssl_ctx,
                                        ticket_keys, sizeof(ticket_keys)))
    {
        printf("SSL_CTX_set_tlsext_ticket_keys failed");
        return -1;
    }

    return 0;
}

UNIFEX_TERM init(UnifexEnv *env, char *local_ip, int local_port, char *remote_ip, int remote_port) {
    printf("local addr %s:%d, remote addr %s:%d\n",local_ip, local_port, remote_ip, remote_port);
    fflush(stdout);
    init_ssl_ctx();
    CNodeState *cnodeState = unifex_alloc_state(env);
    cnodeState->env = env;
    State state;

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = 0;
    local_addr.sin_addr.s_addr = INADDR_ANY;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        printf("Error creating socket\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    if(bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) != 0) {
        printf("Cannot bind");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    if(!memcpy(&state.local_sas, &local_addr, sizeof(local_addr))) {
        printf("memcpy local_sas error\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    state.sockfd = sockfd;


    int flags;
    flags = fcntl(state.sockfd, F_GETFL);
    if (-1 == flags)
        return -1;
    flags |= O_NONBLOCK;
    if (0 != fcntl(state.sockfd, F_SETFL, flags))
        return -1;

    state.peer_addr = new_addr(remote_ip, remote_port);

    struct lsquic_engine_settings engine_settings;
    lsquic_engine_init_settings(&engine_settings, 0);
    engine_settings.es_ecn = 0;

    struct lsquic_engine_api engine_api;
    memset(&engine_api, 0, sizeof(engine_api));
    engine_api.ea_packets_out = send_packets_out;
    engine_api.ea_packets_out_ctx = (void *) &state.sockfd;
    engine_api.ea_stream_if = &stream_if;
    engine_api.ea_stream_if_ctx = (void *) &state;
    engine_api.ea_alpn = "echo";
    engine_api.ea_get_ssl_ctx = get_ssl_ctx;
    engine_api.ea_settings = &engine_settings;

    if (0 != lsquic_global_init(LSQUIC_GLOBAL_CLIENT)) {
        printf("Cannot init\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
//    init_logger("debug");

    create_event_loop(&state);


    state.engine = lsquic_engine_new(0, &engine_api);

    state.conn = lsquic_engine_connect(state.engine, N_LSQVER,
                                                (struct sockaddr *) &state.local_sas,
                                                (struct sockaddr *) &state.peer_addr, (void *) &state.sockfd, NULL,
                                                NULL, 0, NULL, 0, NULL, 0);

    if(!state.conn) {
        printf("Cannot create connection\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    } else {
        printf("Connection created\n");
        fflush(stdout);
    }

    process_conns(&state);
    ev_run (state.loop, 0);

//    pthread_create(&cnodeState->event_loop_tid, NULL, run_ev_thread_fun, (void *)&state);

    return init_result_ok(env, cnodeState);
}

void *run_ev_thread_fun(void *arg) {
    printf("Running new thread\n");
    fflush(stdout);
    State *state = arg;
    ev_run (state->loop, 0);
    return NULL;
}

void handle_destroy_state(UnifexEnv *env, CNodeState *state) {

}
