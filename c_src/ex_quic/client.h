#pragma once

#include <unifex/unifex.h>
#include <pthread.h>

#define EV_STANDALONE 1
#define EV_API_STATIC 1
#include "ev.c"
#include "lsquic_utils.h"

typedef struct State State;

struct State {
    UnifexEnv *env;

    // event loop
    struct ev_loop *loop;
    ev_io sock_watcher;
    ev_timer conn_watcher;
    pthread_t event_loop_tid;

    // lsquic
    int sockfd;
    struct sockaddr_storage local_sas;
    struct sockaddr_in peer_addr;
    lsquic_engine_t *engine;
    lsquic_conn_t *conn;
    lsquic_stream_t *stream;
    struct lsquic_engine_settings engine_settings;
    struct lsquic_engine_api engine_api;

    // msg to send
    char *buf;
    int size;
};

#include "_generated/client.h"

