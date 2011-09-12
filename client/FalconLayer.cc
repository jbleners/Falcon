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
#include "FalconLayer.h"
#include "Watchdog.h"
#include "FalconClient.h"

#include <netinet/in.h>
#include <arpa/inet.h>

namespace {
// RAII to get rid of Boilerplate for RPCs
struct Monitor {
    Monitor(pthread_mutex_t* l, pthread_cond_t* c, bool* cv) :
        lock(l), cond(c), cond_var(cv) {
        pthread_mutex_lock(lock);
        while (*cond_var) {
            pthread_cond_wait(cond, lock);
        }
        *cond_var = true;
        pthread_mutex_unlock(lock);
    }

    ~Monitor() {
        pthread_mutex_lock(lock);
        *cond_var = false;
        pthread_cond_signal(cond);
        pthread_mutex_unlock(lock);
    }

    pthread_mutex_t*    lock;
    pthread_cond_t*     cond;
    bool*               cond_var;
};

const timeval kFalconWait = {10, 0};
const timeval kFalconRetry = {2, 0};
}  // end anonymous namespace

FalconLayer::FalconLayer(const LayerId& h, const Generation& g, bool killable,
                         FalconParentPtr parent, client_addr_t& addr,
                         uint32_t timeout, bool has_watchdog,
                         bool is_base, sockaddr_in* my_addr) :
                                            handle_(h),
                                            gen_(g),
                                            killable_(killable),
                                            base_layer_(is_base),
                                            parent_(parent),
                                            addr_(addr) {
    pthread_mutex_init(&kill_lock_, NULL);
    pthread_mutex_init(&cancel_lock_, NULL);
    pthread_mutex_init(&add_child_lock_, NULL);

    pthread_cond_init(&kill_cond_, NULL);
    pthread_cond_init(&cancel_cond_, NULL);
    pthread_cond_init(&add_child_cond_, NULL);

    kill_active_ = false;
    cancel_active_ = false;
    add_child_active_ = false;

    watchdog_ = NULL;
    clnt_ = NULL;

    if (has_watchdog) {
        watchdog_ = new Watchdog(parent.lock(), handle_, timeout);
    } else if (!base_layer_) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        CHECK(fd_ >= 0);
        clnt_ = clntudp_create(my_addr, SPY_PROG, SPY_V1, kFalconRetry,
                               &fd_);
        CHECK(clnt_);
    }
}

FalconLayer::~FalconLayer() {
    // I'm sorry Old Yeller, I've got to put you down!
    if (watchdog_) {
        watchdog_->Stop();
    }

    // Close the spy client for this layer.
    if (clnt_) {
        clnt_destroy(clnt_);
        close(fd_);
    }

    // Clean up our mutexes. This is auto-destroyed by shared ptr, so we
    // shouldn't need to worry about the mutexes being locked. Nevertheless,
    // we sanity check.
    CHECK(0 == pthread_mutex_destroy(&kill_lock_));
    CHECK(0 == pthread_mutex_destroy(&cancel_lock_));
    CHECK(0 == pthread_mutex_destroy(&add_child_lock_));

    CHECK(0 == pthread_cond_destroy(&kill_cond_));
    CHECK(0 == pthread_cond_destroy(&cancel_cond_));
    CHECK(0 == pthread_cond_destroy(&add_child_cond_));

    children_.clear();
}

void
FalconLayer::DoChildUp(const LayerId& child, Generation gen) {
    LockAll();
    FalconLayerPtr c = children_[child];
    if (!c) {
        LOG("Up for non-existent child %s", child.c_str());
    } else if (c->gen_ == gen) {
        c->SetUpCallbackTime();
    } else {
        LOG("Bad generation for %s", child.c_str());
    }
    UnlockAll();
}

void
FalconLayer::DoChildDown(const LayerId& child, Generation gen,
                         uint32_t falcon_status, uint32_t remote_status) {
    LockAll();
    FalconLayerPtr c = children_[child];
    if (!c) {
        LOG("Down for non-existent child %s", child.c_str());
    } else if (c->gen_ == gen) {
        c->RunCallbacks(child, falcon_status, remote_status);
        children_.erase(child);
    } else {
        LOG("Bad generation for %s", child.c_str());
    }
    UnlockAll();
}

void
FalconLayer::InitRPCArgs(const LayerId& child, target_t* target,
                         client_addr_t *client, const Generation* gen) {
    LockAll();
    target->handle = strndup(child.c_str(), child.size() + 1);
    if (!gen) {
        FalconLayerPtr c = children_[child];
        if (c) {
            c->gen_.SetGen(target);
        }
    } else {
        gen->SetGen(target);
    }
    if (client) {
        *client = addr_;
    }
    UnlockAll();
    return;
}

bool
FalconLayer::CancelChild(const LayerId& child) {
    Monitor m(&cancel_lock_, &cancel_cond_, &cancel_active_);
    if (base_layer_) {
        return false;
    }
    LockAll();
    child_leaves_[child]--;
    int c = child_leaves_[child];
    UnlockAll();
    if (c != 0) return false;
    spy_cancel_arg arg;
    spy_res res;
    memset(&arg, 0, sizeof(arg));
    memset(&res, 0, sizeof(res));
    InitRPCArgs(child, &arg.target, &arg.client, NULL);
    // Cancel. LOG, but ignore error
    clnt_stat st = clnt_call(clnt_, SPY_CANCEL, (xdrproc_t) xdr_spy_cancel_arg,
                            (caddr_t) &arg, (xdrproc_t) xdr_spy_res,
                            (caddr_t) &res, kFalconWait);
    if (st != RPC_SUCCESS || res.status != FALCON_CANCEL_ACK) {
        LOG("cancel error %s: rpc: %d falcon: %d", child.c_str(), st,
            res.status);
    } else {
        LOG("Cancel success %s", child.c_str());
    }

    free(arg.target.handle);
    free(arg.target.generation.generation_val);
    xdr_free((xdrproc_t) xdr_spy_res, reinterpret_cast<char*>(&res));
    return true;
}

int
FalconLayer::KillChild(const LayerId& child) {
    Monitor m(&kill_lock_, &kill_cond_, &kill_active_);
    if (base_layer_) {
        LOG("Trying to kill %s at base layer", child.c_str());
        for (;;);
    }
    // Is it killable
    LockAll();
    FalconLayerPtr c = children_[child];
    UnlockAll();
    if (!c) {
        LOG("Trying to kill non-existent child %s", child.c_str());
        return -1;
    }
    if (!c->killable_) {
        c->RunCallbacks(child, FALCON_E2E_TIMEOUT, 0);
        return 0;
    }

    // Initialize the arguments
    spy_kill_arg arg;
    spy_res res;
    memset(&arg, 0, sizeof(arg));
    memset(&res, 0, sizeof(res));
    InitRPCArgs(child, &arg.target, NULL, NULL);
    clnt_stat st = clnt_call(clnt_, SPY_KILL, (xdrproc_t) xdr_spy_kill_arg,
                            (caddr_t) &arg, (xdrproc_t) xdr_spy_res,
                            (caddr_t) &res, kFalconWait);
    if (st != RPC_SUCCESS || res.status != FALCON_KILL_ACK) {
        LOG("kill error: rpc: %d falcon: %d", st, res.status);
        if (st == RPC_SUCCESS && res.status == FALCON_LONG_DEAD) {
            c->RunCallbacks(child, KILL_LONG_DEAD, 0);
            return 0;
        }
        return 1;
    }

    free(arg.target.handle);
    free(arg.target.generation.generation_val);
    xdr_free((xdrproc_t) xdr_spy_res, reinterpret_cast<char*>(&res));
    return -1;
}

FalconLayerPtr
FalconLayer::AddChild(const LayerId& child, bool killable, client_addr_t& addr,
                      int32_t timeout, bool leaf_layer,
                      FalconParentPtr parent, FalconCallbackPtr cb) {
    Monitor m(&add_child_lock_, &add_child_cond_, &add_child_active_);

    FalconLayerPtr ret = FalconLayerPtr();
    LockAll();
    ret = children_[child];
    UnlockAll();
    if (ret) {
        ret->AddCallback(cb);
        return ret;
    }

    // Step 1. Get an address if it is not a leaf
    sockaddr_in* layer_addr = NULL;
    if (!leaf_layer) {
        addrinfo hints;
        addrinfo* aret = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        if (0 != getaddrinfo(child.c_str(), NULL, &hints, &aret)) {
            LOG("Couldn't get addr for %s", child.c_str());
            return ret;
        }
        layer_addr = new sockaddr_in(*reinterpret_cast<sockaddr_in*>
                                     (aret->ai_addr));
        layer_addr->sin_port = htons(kFalconPort);
        freeaddrinfo(aret);
    }

    // Step 2. If it's the base layer just add the child and return
    if (base_layer_) {
        ret = FalconLayerPtr(new FalconLayer(child, gen_, killable, parent,
                                             addr, timeout, leaf_layer, false,
                                             layer_addr));
        LockAll();
        children_[child] = ret;
        child_leaves_[child]++;
        ret->AddCallback(cb);
        UnlockAll();
        delete layer_addr;
        return ret;
    }

    // Step 3. Get its generation
    spy_get_gen_arg arg;
    spy_res res;
    memset(&arg, 0, sizeof(arg));
    memset(&res, 0, sizeof(res));
    InitRPCArgs(child, &arg.target, NULL, NULL);
    clnt_stat st = clnt_call(clnt_, SPY_GET_GEN,
                             (xdrproc_t) xdr_spy_get_gen_arg, (caddr_t) &arg,
                             (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                             kFalconWait);
    Generation new_gen(res.target.generation.generation_len,
                       res.target.generation.generation_val);
    xdr_free((xdrproc_t) xdr_spy_res, reinterpret_cast<char*>(&res));
    xdr_free((xdrproc_t) xdr_spy_get_gen_arg, reinterpret_cast<char*>(&arg));

    if (st != RPC_SUCCESS || res.status != FALCON_GEN_RESP) {
        LOG("Error getting gen for %s at %s: rpc: %d falcon: %d", child.c_str(),
            handle_.c_str(), st, res.status);
        delete layer_addr;
        return ret;
    }
    if (!IsChild(gen_, new_gen)) {
        LOG("Got bad generation!");
        delete layer_addr;
        return ret;
    }

    // Step 4. Register, construct and return
    spy_register_arg rarg;
    memset(&rarg, 0, sizeof(rarg));
    memset(&res, 0, sizeof(res));
    InitRPCArgs(child, &rarg.target, &rarg.client, &new_gen);
    rarg.lethal = killable;
    if (leaf_layer && timeout >= 0) {
        rarg.up_interval_ms = kSecondsToMilliseconds * timeout;
    } else {
        rarg.up_interval_ms = -1;
    }
    st = clnt_call(clnt_, SPY_REGISTER, (xdrproc_t) xdr_spy_register_arg,
                   (caddr_t) &rarg, (xdrproc_t) xdr_spy_res, (caddr_t) &res,
                   kFalconWait);

    if (st == RPC_SUCCESS && res.status == FALCON_REGISTER_ACK) {
        ret = FalconLayerPtr(new FalconLayer(child, new_gen, killable, parent,
                                             addr, timeout, leaf_layer, false,
                                             layer_addr));
        LockAll();
        children_[child] = ret;
        child_leaves_[child]++;
        ret->AddCallback(cb);
        UnlockAll();
    } else {
        LOG("Failed to register %s: rpc: %d falcon: %d", child.c_str(),
                st, res.status);
    }

    xdr_free((xdrproc_t) xdr_spy_res, reinterpret_cast<char*>(&res));
    xdr_free((xdrproc_t) xdr_spy_register_arg, reinterpret_cast<char*>(&rarg));
    delete layer_addr;
    return ret;
}

void
FalconLayer::AddCallback(FalconCallbackPtr cb) {
    cb_list_.Add(cb);
}

FalconLayerPtr
FalconLayer::GetParent() const {
    return parent_.lock();
}

const LayerId&
FalconLayer::GetLayerId() const {
    return handle_;
}

inline void
FalconLayer::SetUpCallbackTime() {
    if (watchdog_) {
        watchdog_->Pet();
    } else {
        LOG1("Got up for layer without watchdog");
    }
}

void
FalconLayer::StartTimer(FalconCallbackPtr cb, int timeout) {
    if (watchdog_) {
        watchdog_->StartTimer(cb, timeout);
    } else {
        LOG("Trying to start timer on layer w/o watchdog");
    }
}

void
FalconLayer::StopTimer() {
    if (watchdog_) {
        watchdog_->StopTimer();
    } else {
        LOG("Trying to stop timer on layer w/o watchdog");
    }
}

void
FalconLayer::Cancel() {
    if (watchdog_) {
        watchdog_->Cancel();
    } else {
        LOG("Trying to cancel non-top layer.");
    }
}

// cb_list_ does its own synchronization.
inline void
FalconLayer::RunCallbacks(const LayerId& lid, uint32_t falcon_status,
                          uint32_t remote_status) {
    return cb_list_.Run(lid, falcon_status, remote_status);
}

inline void
FalconLayer::LockAll() {
    pthread_mutex_lock(&kill_lock_);
    pthread_mutex_lock(&cancel_lock_);
    pthread_mutex_lock(&add_child_lock_);
}

inline void
FalconLayer::UnlockAll() {
    pthread_mutex_unlock(&add_child_lock_);
    pthread_mutex_unlock(&cancel_lock_);
    pthread_mutex_unlock(&kill_lock_);
}
