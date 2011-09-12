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
#include <sched.h>

#include <arpc.h>
#include <async.h>
#include <callback.h>

#include "enforcer.h"
#include "client_prot.h"

#include "common.h"

bool operator<(const ref<FalconClient> c1, const ref<FalconClient> c2) {
    return *c1 < *c2;
}

Enforcer::Enforcer() : logfile_name_("/dev/null"), generation_log_(NULL) {}

void
Enforcer::Run() {
    // Initialize generations from file
    InitGenerations();
    // Start server
    int fd = inetsocket(SOCK_DGRAM, kFalconPort, INADDR_ANY);
    CHECK(fd >= 0);
    make_async(fd);
    close_on_exec(fd);
    srv_ = asrv::alloc(axprt_dgram::alloc(fd), spy_prog_1);
    srv_->setcb(wrap(mkref(this), &Enforcer::Dispatch));
    amain();
}

void
Enforcer::InitGenerations() {
    generation_log_ = fopen(logfile_name_, "a+");
    CHECK(generation_log_);
    const size_t kTargetSize = 256;
    char target[kTargetSize];
    while (NULL != fgets(target, kTargetSize, generation_log_)) {
        size_t new_line = strlen(target) - 1;
        target[new_line] = '\0';
        generations_[target]++;
    }
    return;
}

void
Enforcer::IncrementGeneration(const ref<const str> target) {
    const char* buf = target->cstr();
    CHECK(0 <= fprintf(generation_log_, "%s\n", buf));
    CHECK(0 == fflush(generation_log_));
    if (0 == fsync(fileno(generation_log_)) ||
        EROFS == errno || EINVAL == errno) {
        generations_[*target]++;
    } else {
        CHECK(0);
    }
}

void
Enforcer::FastForwardGeneration(const ref<const str> target, uint32_t new_gen) {
    // In the non-lethal case
    if (new_gen < generations_[*target]) {
        LOG("Setting generation %d %d", new_gen, generations_[*target]);
        generations_[*target] = new_gen;
    }
    while (new_gen > generations_[*target]) {
        IncrementGeneration(target);
    }
}

// Taken from www-theorie.physik.unizh.ch/~dpotter/howto/daemonize
void
Enforcer::Daemonize(const char* logfile) {
    pid_t pid, sid;
    if (getppid() == 1) return;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    sid = setsid();
    if (sid < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    freopen("/dev/null", "r", stdin);
    freopen(logfile, "a", stdout);
    freopen(logfile, "a", stderr);
    CHECK(0 == fcntl(0, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(1, F_SETFL, O_NONBLOCK));
    CHECK(0 == fcntl(2, F_SETFL, O_NONBLOCK));
    return;
}

void
Enforcer::Prioritize() {
    // Get the priority.
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    CHECK(0 == sched_setscheduler(0, SCHED_FIFO, &param));
    CHECK(0 == nice(-20) || -20 == nice(-20));

    // Get the memory
    struct rlimit rlim;
    rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
    CHECK(0 == setrlimit(RLIMIT_MEMLOCK, &rlim));
    CHECK(0 == mlockall(MCL_CURRENT | MCL_FUTURE));
    return;
}

const int32_t kClientTimeout = 90;  // Test every 1.5 minutes
const int32_t kClientRetry = 5;  // Will remove after 7.5 minutes
void
Enforcer::DoHeartbeat(const ref<FalconClient> cl, int retry_count) {
    cl->clnt_->timedcall(kClientTimeout, CLIENT_NULL, NULL, NULL,
                         wrap(mkref(this), &Enforcer::HandleClientHeartbeat,
                              cl, retry_count));
    return;
}

void
Enforcer::HandleClientHeartbeat(const ref<FalconClient> cl,
                                int retry_count,
                                clnt_stat status) {
    if (all_clients_.count(cl) == 0) return;
    if (status == RPC_SUCCESS || retry_count < kClientRetry) {
        if (status == RPC_SUCCESS) {
            delaycb(kClientTimeout, wrap(mkref(this), &Enforcer::DoHeartbeat,
                                         cl, 0));
        } else {
            retry_count++;
            DoHeartbeat(cl, retry_count);
        }
    } else {
        RemoveClient(cl);
    }
    return;
}

const int32_t kClientStrBuf = 64;
ref<FalconClient>
Enforcer::GetClient(const client_addr_t& addr) {
    ref<FalconClient> new_client = New refcounted<FalconClient>;
    // Create id string from addr
    char buf[kClientStrBuf];
    in_addr a;
    a.s_addr = addr.ipaddr;
    snprintf(buf, kClientStrBuf, "%s:%u:%u", inet_ntoa(a),
             ntohs(addr.port), addr.client_tag);
    new_client->id = buf;
    new_client->client_tag_ = addr.client_tag;
    // Check if id is in place
    if (all_clients_.count(new_client) == 0) {
        LOG("Adding new client: %s", buf);
        int cfd = inetsocket(SOCK_DGRAM, 0, 0);
        CHECK(cfd >= 0);
        make_async(cfd);
        close_on_exec(cfd);
        struct sockaddr_in iaddr;
        memset(&iaddr, 0, sizeof(iaddr));
        iaddr.sin_family = AF_INET;
        iaddr.sin_addr.s_addr = addr.ipaddr;
        iaddr.sin_port = addr.port;
        ptr<aclnt> clnt = aclnt::alloc(axprt_dgram::alloc(cfd), client_prog_1,
                                reinterpret_cast<sockaddr *>(&iaddr),
                                callbase_alloc<rpccb_unreliable>);
        new_client->clnt_ = clnt;
        all_clients_.insert(new_client);
        DoHeartbeat(new_client, 0);
    }
    return *(all_clients_.find(new_client));
}

void
Enforcer::RemoveClient(const ref<FalconClient> c) {
    LOG("Remvoing client %s", c->id.cstr());
    all_clients_.erase(c);
    std::map<str, int32_t>::iterator it;
    for (it = c->up_intervals_.begin(); it != c->up_intervals_.end(); ++it) {
        target_clients_[it->first].erase(c);
        deadly_[it->first].erase(c);
        if (target_clients_[it->first].empty()) {
            ref<str> target = New refcounted<str>(it->first);
            StopMonitoring(target);
        }
    }
    return;
}

void
Enforcer::ObserveUp(const ref<const str> target) {
    std::set<ref<FalconClient> > fc_set = waiting_for_up_[*target];
    waiting_for_up_[*target].clear();
    std::set<ref<FalconClient> >::iterator it;
    for (it = fc_set.begin(); it != fc_set.end(); ++it) {
        client_up_arg a;
        a.handle = *target;
        a.generation = gen_vec_;
        a.generation.push_back(generations_[*target]);
        a.client_tag = (*it)->client_tag_;
        (*it)->clnt_->call(CLIENT_UP, &a, NULL, aclnt_cb_null);
        RepeatWaiting(target, *it);
    }
    return;
}

const int kMaxRetries = 3;
void
Enforcer::ReportDown(const ref<FalconClient> cl, const ref<client_down_arg> arg,
                     int32_t retries, clnt_stat status) {
    if (status == RPC_SUCCESS && retries != 0) {
        cl->up_intervals_.erase(arg->handle);
        target_clients_[arg->handle].erase(cl);
        deadly_[arg->handle].erase(cl);
        if (cl->up_intervals_.empty()) {
            RemoveClient(cl);
        }
        LOG("Successfully sent down %s", cl->id.cstr());
        return;
    } else if (retries == kMaxRetries) {
        LOG1("Removing client");
        RemoveClient(cl);
        return;
    }
    LOG("Doing %d down call for %s", retries + 1,  cl->id.cstr());
    cl->clnt_->call(CLIENT_DOWN, arg, NULL, wrap(mkref(this),
                    &Enforcer::ReportDown, cl, arg, retries + 1));
    return;
}

void
Enforcer::ObserveDown(const ref<const str> target,
                      const uint32_t status,
                      const bool killed,
                      const bool would_kill) {
    LOG("%s", target->cstr());
    // Get clients to contact
    std::set<ref<FalconClient> >& fc_set = target_clients_[*target];
    std::set<ref<FalconClient> >::iterator it;

    // Send response
    // TODO(leners) add retries.
    for (it = fc_set.begin(); it != fc_set.end(); ++it) {
        // Construct response
        ref<client_down_arg> a = New refcounted<client_down_arg>;
        a->handle = *target;
        a->generation = gen_vec_;
        a->generation.push_back(generations_[*target]);
        a->layer_status = status;
        a->killed = killed;
        a->would_kill = would_kill;
        a->client_tag = (*it)->client_tag_;

        LOG("Down call for: %s", (*it)->id.cstr());
        ReportDown(*it, a, 0, RPC_SUCCESS /* this arg is ignored */);
    }
    IncrementGeneration(target);

    // The layer is gone, there are no more clients to give
    fc_set.clear();
    waiting_for_up_[*target].clear();
    StopMonitoring(target);
    return;
}

void
Enforcer::AddToWaiting(const ref<const str> target,
                       const ref<FalconClient> client) {
    if (1 == target_clients_[*target].count(client)) {
        waiting_for_up_[*target].insert(client);
    }
    return;
}

void
Enforcer::RepeatWaiting(const ref<const str> target,
                        const ref<FalconClient> client) {
    if (1 == target_clients_[*target].count(client)) {
        int32_t delay = client->up_intervals_[*target];
        if (delay < 0) {
            return;
        }
        if (delay == 0) {
            AddToWaiting(target, client);
            return;
        }
        int32_t delay_s = delay / kSecondsToMilliseconds;
        int32_t delay_ns = (delay % kSecondsToMilliseconds) *
                           kMillisecondsToNanoseconds;
        delaycb(delay_s, delay_ns,
                wrap(mkref(this), &Enforcer::AddToWaiting,
                     target, client));
    }
    return;
}

spy_status
Enforcer::GenCheck(const ref<const str> target,
                   const rpc_vec<gen_no, RPC_INFINITY>& query_gen) {
    UpdateGenerations(target);
    rpc_vec<gen_no, RPC_INFINITY> my_vec = gen_vec_;
    my_vec.push_back(generations_[*target]);
    // Check whether we are actually monitoring the target
    if (InvalidTarget(target)) {
        return FALCON_UNKNOWN_TARGET;
    }
    // Sanity check generation vector
    if (my_vec.size() != query_gen.size()) {
        return FALCON_BAD_GEN_VEC;
    }
    // Check Generation vector
    for (unsigned int i = 0; i < query_gen.size(); ++i) {
        if (my_vec[i] < query_gen[i]) {
            LOG("Future gen %i me: %d them: %d", i, my_vec[i], query_gen[i]);
            return FALCON_FUTURE_GEN;
        } else if (my_vec[i] > query_gen[i]) {
            return FALCON_LONG_DEAD;
        }
    }
    return FALCON_SUCCESS;
}

void
Enforcer::Dispatch(svccb *sbp) {
    switch (sbp->proc()) {
        case SPY_NULL:
            sbp->reply(0);
            break;
        case SPY_REGISTER: {
            spy_res res;
            spy_register_arg *argp = sbp->Xtmpl getarg<spy_register_arg> ();
            const ref<str> target = New refcounted<str>(argp->target.handle);
            res.target.handle = argp->target.handle;
            rpc_vec<gen_no, RPC_INFINITY> my_vec = gen_vec_;
            my_vec.push_back(generations_[res.target.handle]);
            res.target.generation = my_vec;
            res.status = GenCheck(target, argp->target.generation);
            if (res.status != FALCON_SUCCESS) {
                sbp->reply(&res);
                LOG("Gen error target %s", target->cstr());
                return;
            }
            // Add the client (if necessary)
            ref<FalconClient> cl = GetClient(argp->client);
            bool new_client = target_clients_[*target].insert(cl).second;
            int32_t delay = argp->up_interval_ms;
            cl->up_intervals_[*target] = delay;
            RepeatWaiting(target, cl);
            if (argp->lethal) {
                deadly_[*target].insert(cl);
            }
            if (target_clients_[*target].size() == 1 && new_client) {
                StartMonitoring(target);
            }
            res.status = FALCON_REGISTER_ACK;
            sbp->reply(&res);
            return;
        }
        case SPY_CANCEL: {
            spy_res res;
            spy_cancel_arg *argp = sbp->Xtmpl getarg<spy_cancel_arg> ();
            const ref<str> target = New refcounted<str>(argp->target.handle);
            res.target.handle = argp->target.handle;
            rpc_vec<gen_no, RPC_INFINITY> my_vec = gen_vec_;
            my_vec.push_back(generations_[res.target.handle]);
            res.target.generation = my_vec;
            res.status = GenCheck(target, argp->target.generation);
            if (res.status != FALCON_SUCCESS) {
                LOG("tried to cancel %s, but couldn't", target->cstr());
                sbp->reply(&res);
                return;
            }
            ref<FalconClient> cl = GetClient(argp->client);
            if (target_clients_[*target].erase(cl) == 1) {
                cl->up_intervals_.erase(*target);
                deadly_[*target].erase(cl);
                res.status = FALCON_CANCEL_ACK;
                if (target_clients_[*target].empty()) {
                    LOG("Cancled %s", target->cstr());
                    StopMonitoring(target);
                } else {
                    LOG("Would cancel %s, but there are other clients",
                        target->cstr());
                }
                if (cl->up_intervals_.size() == 0) {
                    RemoveClient(cl);
                } else {
                   LOG("At least %s is still in the map.",
                        cl->up_intervals_.begin()->first.cstr());
                }
            } else {
                res.status = FALCON_CANCEL_ERROR;
            }
            sbp->reply(&res);
            return;
        }
        case SPY_KILL: {
            spy_res res;
            spy_kill_arg *argp = sbp->Xtmpl getarg<spy_kill_arg> ();
            const ref<str> target = New refcounted<str>(argp->target.handle);
            res.target.handle = argp->target.handle;
            rpc_vec<gen_no, RPC_INFINITY> my_vec = gen_vec_;
            my_vec.push_back(generations_[*target]);
            res.target.generation = my_vec;
            res.status = GenCheck(target, argp->target.generation);
            if (res.status != FALCON_SUCCESS) {
                sbp->reply(&res);
                return;
            }
            Kill(target);
            res.status = FALCON_KILL_ACK;
            sbp->reply(&res);
            return;
        }
        case SPY_GET_GEN: {
            spy_res res;
            spy_kill_arg *argp = sbp->Xtmpl getarg<spy_kill_arg> ();
            const ref<str> target = New refcounted<str>(argp->target.handle);
            res.target.handle = argp->target.handle;
            if (InvalidTarget(target)) {
                res.status = FALCON_UNKNOWN_TARGET;
                LOG("Unknown target %s", target->cstr());
            } else {
                UpdateGenerations(target);
                rpc_vec<gen_no, RPC_INFINITY> my_vec = gen_vec_;
                my_vec.push_back(generations_[*target]);
                res.target.generation = my_vec;
                res.status = FALCON_GEN_RESP;
            }
            sbp->reply(&res);
            return;
        }
    }
    return;
}

Enforcer::~Enforcer() {
    return;
}
