#pragma once

#include <unifex/unifex.h>
#include <pthread.h>

typedef struct CNodeState CNodeState;

struct CNodeState {
    UnifexEnv *env;
    pthread_t event_loop_tid;
};

#include "_generated/client.h"

