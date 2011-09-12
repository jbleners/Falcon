/* 
 * Copyright (c) 2011 Joshua B. Leners (University of Texas at Austin).
 * All rights reserved.
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Texas at Austin. The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 *
 */
#include "process_spy/process_observer.h"

#include <pthread.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
const uint32_t kDefaultDelay_ms = 100;
spyfunc_f Spy;
uint32_t delay_ms_ = kDefaultDelay_ms;
delay_type delay_;
char* handle_ = NULL;

void *
SpyThread(void *) {
    struct process_observer_handshake handshake;
    strncpy(handshake.handle, reinterpret_cast<char *>(handle_), kHandleSize);
    handshake.pid = getpid();
    handshake.delay = delay_;
    handshake.delay_ms = delay_ms_;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
             falcon_process_enforcer_socket);
    for (;;) {
        int handlerd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        CHECK(handlerd_socket > 0);
        CHECK(0 == connect(handlerd_socket, (struct sockaddr *) &addr,
                           sizeof(addr)));
        CHECK(sizeof(handshake) == send(handlerd_socket, &handshake,
                                        sizeof(handshake), 0));
        struct process_observer_probe probe;
        for (;;) {
            if (sizeof(probe) !=
                    recv(handlerd_socket, &probe, sizeof(probe), 0)) break;
            struct process_observer_reply reply;
            reply.state = Spy();
            if (sizeof(reply) !=
                    send(handlerd_socket, &reply, sizeof(reply), 0)) break;
        }
        close(handlerd_socket);
    }
    return NULL;
}
}  // end anonymous namespace

void
SetSpy(spyfunc_f spy, const char* handle) {
    SetSpy(spy, handle, DELAY_REALTIME, kDefaultDelay_ms);
}

void
SetSpy(spyfunc_f spy, const char *handle, delay_type delay, uint32_t delay_ms) {
    Spy = spy;
    delay_ = delay;
    delay_ms_ = delay_ms;
    handle_ = strndup(handle, kHandleSize);
    pthread_t thread;
    CHECK(0 == pthread_create(&thread, NULL, SpyThread, NULL));
    return;
}

void
SetHandle(const char * handle) {
    CHECK(0 == prctl(PR_SET_NAME, handle, NULL, NULL));
    return;
}
