#include <stdio.h>
#include <lsquic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <openssl/ssl.h>
#include <openssl/crypto.h>

#include "client.h"

static int send_packets_out(void *ctx, const struct lsquic_out_spec *specs, unsigned n_specs)
{
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
    State *state = ea_stream_if_ctx;
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
            printf("Connected\n");
            fflush(stdout);
            lsquic_conn_make_stream(state->conn);
            break;
        default:
            printf("Handshake failed\n");
            fflush(stdout);
            break;
    }
}

static lsquic_stream_ctx_t *on_new_stream_cb(void *ea_stream_if_ctx, lsquic_stream_t *stream) {
    State *state = ea_stream_if_ctx;
    state->stream = stream;
    return (void *) state;
}

static void on_read_cb(lsquic_stream_t *stream, lsquic_stream_ctx_t *h) {
    lsquic_conn_t *conn = lsquic_stream_conn(stream);
    State *state = (void *) lsquic_conn_get_ctx(conn);

    unsigned char buf[256] = {0};

    ssize_t nr = lsquic_stream_read(stream, buf, sizeof(buf));

    buf[nr] = '\0';
    UnifexPayload *payload = unifex_payload_alloc(state->env, UNIFEX_PAYLOAD_BINARY, nr);
    memcpy(payload->data, buf, nr);
    send_quic_payload(state->env, *state->env->reply_to, 0, payload);

    lsquic_stream_wantread(stream, 0);
}

static void on_write_cb(lsquic_stream_t *stream, lsquic_stream_ctx_t *h) {
    lsquic_conn_t *conn = lsquic_stream_conn(stream);
    State *state = (void *) lsquic_conn_get_ctx(conn);
    lsquic_stream_write(stream, state->buf, state->size);
    lsquic_stream_wantwrite(stream, 0);
    lsquic_stream_flush(stream);
    lsquic_stream_wantread(stream, 1);
}

void create_event_loop(State *state) {
    state->loop = EV_DEFAULT;
    state->sock_watcher.data = state;
    state->conn_watcher.data = state;
    ev_io_init (&state->sock_watcher, read_sock, state->sockfd, EV_READ);
    ev_io_start (state->loop, &state->sock_watcher);
    ev_init(&state->conn_watcher, process_conns_cb);
}

void *run_ev_thread_fun(void *arg) {
    State *state = arg;
    ev_run (state->loop, 0);
    return NULL;
}

const struct lsquic_stream_if stream_if = {
        .on_new_conn            = on_new_conn_cb,
        .on_conn_closed         = on_conn_closed_cb,
        .on_new_stream          = on_new_stream_cb,
        .on_read                = on_read_cb,
        .on_write               = on_write_cb,
        .on_hsk_done            = on_hsk_done
};

UNIFEX_TERM init(UnifexEnv *env, char *remote_ip, int remote_port) {
    State *state = unifex_alloc_state(env);
    memset(state, 0, sizeof(*state));
    state->env = env;
    state->buf = (char *)calloc(256, sizeof(char));
    state->size = 0;

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

    if(!memcpy(&state->local_sas, &local_addr, sizeof(local_addr))) {
        printf("memcpy local_sas error\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    state->sockfd = sockfd;

    state->peer_addr.sin_family = AF_INET;
    state->peer_addr.sin_port = htons(remote_port);
    state->peer_addr.sin_addr.s_addr = inet_addr(remote_ip);

    lsquic_engine_init_settings(&state->engine_settings, 0);
    state->engine_settings.es_ecn = 0;

    state->engine_api.ea_packets_out = send_packets_out;
    state->engine_api.ea_packets_out_ctx = (void *) &state->sockfd;
    state->engine_api.ea_stream_if = &stream_if;
    state->engine_api.ea_stream_if_ctx = (void *) state;
    state->engine_api.ea_alpn = "echo";
    state->engine_api.ea_settings = &state->engine_settings;

    if (0 != lsquic_global_init(LSQUIC_GLOBAL_CLIENT)) {
        printf("Cannot init\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

//    init_logger("debug");

    create_event_loop(state);

    state->engine = lsquic_engine_new(0, &state->engine_api);
    state->conn = lsquic_engine_connect(state->engine, N_LSQVER,
                                                (struct sockaddr *) &state->local_sas,
                                                (struct sockaddr *) &state->peer_addr, (void *) &state->sockfd, NULL,
                                                NULL, 0, NULL, 0, NULL, 0);

    if(!state->conn) {
        printf("Cannot create connection\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    process_conns(state);

    pthread_create(&state->event_loop_tid, NULL, run_ev_thread_fun, (void *)state);
    return init_result_ok(env, state);
}

UNIFEX_TERM send_payload(UnifexEnv *env, State *state, UnifexPayload *payload) {
  memcpy(state->buf, payload->data, payload->size);
  state->size = payload->size;
  lsquic_stream_wantwrite(state->stream, 1);
  process_conns(state);
  return send_payload_result_ok(env, state);
}

void handle_destroy_state(UnifexEnv *env, State *state) {}
