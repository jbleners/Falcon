/* 
 * Copyright (C) 2011 Joshua B. Leners <leners@cs.utexas.edu> and
 * the University of Texas at Austin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include "vmm_spy/vmm_enforcer.h"

#include "async.h"
#include "arpc.h"
#include "binary_libs/libbridge.h"

#include "common.h"
#include "config.h"
#include "enforcer/enforcer.h"
#include "vmm_spy/obs_prot.h"
#include "vmm_spy/util.h"

// parameter namespace
namespace {

const uint32_t  kVMMCheck_ms = 100;
const uint32_t  kVMMResp_us = 20000;
const uint32_t  kVMMRetry = 5;
const int32_t   kMaxPollPeriod_ms = 6000;

uint32_t max_vmm_retry_ = 0;
uint32_t vmm_check_ns_ = 0;
uint32_t vmm_check_s_ = 0;
uint32_t vmm_resp_ns_ = 0;
uint32_t vmm_resp_s_ = 0;
uint32_t vmm_to_s_ = 0;
uint32_t vmm_to_ns_ = 0;
double max_poll_period_ = 0.0;
uint16_t vmm_obs_probe_port_ = 0;

}  // end anonymous namespace

class VMM : public virtual refcount {
  public:
    VMM(ref<str> h, ref<str> v, uint32_t i, uint32_t p, ref<aclnt> c,
        timespec n) :
         handle(h), vlan_id(v), ipaddr(i), switch_port(p), clnt(c),
         monitored(false), last_query(n), count(0) {}
    ref<str>    handle;
    // Use vlan_id for counting packets against netstat.
    ref<str>    vlan_id;
    uint32_t    ipaddr;
    uint32_t    switch_port;
    ref<aclnt>  clnt;
    bool        monitored;
    timespec    last_query;
    uint64_t    count;
};

class VMMEnforcer : public virtual Enforcer {
  public:
    virtual ~VMMEnforcer() {}

    virtual void Init() {
        // Initialize bridging stuff
        br_init();

        // Load appropriate config stuff
        Config::GetFromConfig("vmm_retry", &max_vmm_retry_, kVMMRetry);

        uint32_t check_us = 0;
        Config::GetFromConfig("check_us", &check_us, kVMMResp_us);
        vmm_check_ns_ = (check_us * kMicrosecondsToNanoseconds) %
                        kSecondsToNanoseconds;
        vmm_check_s_ = (check_us * kMicrosecondsToNanoseconds) /
                       kSecondsToNanoseconds;

        uint32_t resp_us = 0;
        Config::GetFromConfig("resp_us", &resp_us, kVMMResp_us);

        vmm_resp_ns_ = (resp_us * kMicrosecondsToNanoseconds) %
                       kSecondsToNanoseconds;
        vmm_resp_s_ = (resp_us * kMicrosecondsToNanoseconds) /
                      kSecondsToNanoseconds;

        vmm_to_ns_ = (resp_us * (max_vmm_retry_ + 1) *
                      kMicrosecondsToNanoseconds) %
                     kSecondsToNanoseconds;

        vmm_to_s_ = (resp_us * (max_vmm_retry_ + 1) *
                     kMicrosecondsToNanoseconds) /
                     kSecondsToNanoseconds;
        int32_t poll_period_ms = 0;
        Config::GetFromConfig("max_poll_period_ms", &poll_period_ms,
                              kMaxPollPeriod_ms);
        max_poll_period_ = (poll_period_ms < 0) ?
                           HUGE_VAL : poll_period_ms * kMillisecondsToSeconds;

        Config::GetFromConfig("vmm_obs_probe_port", &vmm_obs_probe_port_,
                              kDefaultVMMProbePort);

        // start server
        int fd = inetsocket(SOCK_DGRAM, vmm_obs_probe_port_, INADDR_ANY);
        CHECK(fd > 0);
        make_async(fd);
        close_on_exec(fd);
        obs_srv_ = asrv::alloc(axprt_dgram::alloc(fd), vmm_obs_prog_1);
        obs_srv_->setcb(wrap(mkref(this), &VMMEnforcer::ObserverDispatch));

        logfile_name_ = "/jffs/falcon.gen";
        return;
    }

    virtual void StartMonitoring(const ref<const str> handle) {
        LOG("START MONITORING %s", handle->cstr());
        ptr<VMM> target = monitored_vmms_[*handle];
        CHECK(target);
        target->monitored = true;
        ref<obs_probe_msg> msg = New refcounted<obs_probe_msg>();
        msg->counter = target->count;
        ref<rpccb_unreliable*> cb = New refcounted<rpccb_unreliable*>();
        *cb = NULL;
        MonitorAction(handle, GetRXBytes(handle), msg, cb, RPC_SUCCESS);
        return;
    }

    virtual void StopMonitoring(const ref<const str> handle) {
        ptr<VMM> target = monitored_vmms_[*handle];
        CHECK(target);
        if (target->monitored) {
            LOG("STOP MONITORING %s", handle->cstr());
            target->monitored = false;
        }
        return;
    }

    virtual bool InvalidTarget(const ref<const str> handle) {
        return (monitored_vmms_.count(*handle) == 0);
    }

    virtual void Kill(const ref<const str> handle) {
        ptr<VMM> target = monitored_vmms_[*handle];
        CHECK(target);
        stop_port(target->switch_port);
        return;
    }

    virtual void UpdateGenerations(const ref<const str> handle) {
        // No need to run this since we directly control obs/kill
        return;
    }


  private:
    void ObserverDispatch(svccb *sbp) {
        switch (sbp->proc()) {
            case VMM_OBS_NULL:
                sbp->reply(0);
                return;
            case VMM_OBS_REGISTER: {
                LOG("Registering!");
                obs_register_arg *argp =
                                    sbp->Xtmpl getarg<obs_register_arg> ();
                const ref<str> handle = New refcounted<str>(argp->handle);

                // (1) Check hostname sanity (this is from who they say they
                // are dns-wise)
                if (!check_sanity(handle->cstr(),
                                  (const sockaddr_in *) sbp->getsa())) {
                    sbp->ignore();
                }

                // (2) Create client
                sockaddr_in clnt_addr(*((const sockaddr_in *) sbp->getsa()));
                int fd = inetsocket(SOCK_DGRAM, 0, 0);
                clnt_addr.sin_port = htons(kDefaultVMMProbePort);
                make_async(fd);
                close_on_exec(fd);
                ref<aclnt> clnt =
                    aclnt::alloc(axprt_dgram::alloc(fd), vmm_obs_prog_1,
                                 reinterpret_cast<sockaddr *>(&clnt_addr),
                                 callbase_alloc<rpccb_unreliable>);

                // (3) Create and add vmm
                timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                uint32_t ipaddr = clnt_addr.sin_addr.s_addr;
                int32_t switch_port = get_port_from_ip(ipaddr);
                ref<str> vlan_id =
                    New refcounted<str>(get_vlan_from_port(switch_port));
                ptr<VMM> new_vmm = New refcounted<VMM>(handle, vlan_id, ipaddr,
                                                       switch_port, clnt, now);
                ptr<VMM> old_vmm = monitored_vmms_[*(new_vmm->handle)];
                if (old_vmm) {
                    old_vmm->monitored = false;
                }
                monitored_vmms_[*(new_vmm->handle)] = new_vmm;
                obs_register_response r;
                r.generation = GetGeneration(handle);
                sbp->reply(&r);
                return;
            }
            case VMM_OBS_CANCEL: {
                obs_cancel_arg *argp =
                                    sbp->Xtmpl getarg<obs_cancel_arg> ();
                const ref<str> handle = New refcounted<str>(argp->handle);
                if (monitored_vmms_[*handle]) {
                    StopMonitoring(handle);
                    // TODO(leners): is this the right behavior? Should we kill?
                    ObserveDown(handle, VMM_CANCELED, false, false);
                } else {
                    // ignore request if the target isn't monitored.
                    sbp->ignore();
                }
             }
            case VMM_OBS_PROBE:
            default:
                sbp->ignore();
                return;
        }
        return;
    }

    void Retry(ref<VMM> vmm, ref<rpccb_unreliable *> cb, uint32_t retries) {
        if (vmm->monitored && *cb && retries < kVMMRetry) {
            LOG("retrying %s", vmm->handle->cstr());
            (*cb)->xmit(retries + 1);
            delaycb(vmm_resp_s_, vmm_resp_ns_, wrap(mkref(this),
                    &VMMEnforcer::Retry, vmm, cb, retries + 1));
        }
    }

    void MonitorAction(const ref<const str> handle, unsigned long rx_bytes,
                       ref<obs_probe_msg> msg, ref<rpccb_unreliable*> old_cb,
                       clnt_stat st) {
        ptr<VMM> vmm = monitored_vmms_[*handle];
        CHECK(vmm);
        if (!vmm->monitored) return;
        // Two cases, each with two subcases:
        // (1) Timedout RPC call
        //     (a) Try again
        //     (b) Declare death
        // (2) Fired timer
        //     (a) Wait some more
        //     (b) Send a probe for good measure
        if (st == RPC_TIMEDOUT) {
            LOG("Timedout");
            bool killable = Killable(handle);
            if (killable) Kill(handle);
            *old_cb = NULL;
            ObserveDown(handle, VMM_TIMEDOUT, killable, true);
            return;
        } else {
            // Ignore old probes, they are not signs of life
            if (msg->counter < vmm->count) {
                return;
            }

            // Acknowledge the up, clear the clnt.
            CHECK(st == RPC_SUCCESS);
            ObserveUp(handle);
            *old_cb = NULL;

            unsigned long new_rx_bytes = GetRXBytes(handle);
            timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            // If it's been a while since the last probe or we haven't seen
            // any network activity, send a probe
            if (TimespecDiff(&now, &vmm->last_query) > max_poll_period_ ||
                new_rx_bytes == rx_bytes) {
                ref<obs_probe_msg> arg = New refcounted<obs_probe_msg>();
                ref<obs_probe_msg> res = New refcounted<obs_probe_msg>();

                arg->counter = vmm->count;
                vmm->count++;
                ref<rpccb_unreliable*> cb =
                    New refcounted<rpccb_unreliable*>();
                rpccb_unreliable* rcb = static_cast<rpccb_unreliable*>(
                                        vmm->clnt->timedcall(vmm_to_s_,
                                        vmm_to_ns_, VMM_OBS_PROBE, arg, res,
                                        wrap(mkref(this),
                                           &VMMEnforcer::MonitorAction, handle,
                                           new_rx_bytes, res, cb)));
                *cb = rcb;
                delaycb(vmm_resp_s_, vmm_resp_ns_, wrap(mkref(this),
                        &VMMEnforcer::Retry, vmm, cb, 0));
                clock_gettime(CLOCK_REALTIME, &vmm->last_query);
            } else {
                delaycb(vmm_check_s_, vmm_check_ns_,
                        wrap(mkref(this), &VMMEnforcer::MonitorAction,
                             handle, new_rx_bytes, msg, old_cb, RPC_SUCCESS));
            }
        }
        return;
    }

    unsigned long GetRXBytes(const ref<const str> vm_name) {
        // Adapted from net-tools iptunnel.c code in gentoo portage
        ref<str> ifname = monitored_vmms_[*vm_name]->vlan_id;
        char name[IFNAMSIZ];
        unsigned long  rx_bytes, rx_packets, rx_errs, rx_drops,
        rx_fifo, rx_frame,
        tx_bytes, tx_packets, tx_errs, tx_drops,
        tx_fifo, tx_colls, tx_carrier, rx_multi;
        char buf[512];
        FILE *fp = fopen("/proc/net/dev", "r");
        CHECK(fp);

        fgets(buf, sizeof(buf), fp);
        fgets(buf, sizeof(buf), fp);
        bool found = false;
        while (fgets(buf, sizeof(buf), fp) != NULL) {
                char *ptr;
                buf[sizeof(buf) - 1] = 0;
                CHECK(!((ptr = strchr(buf, ':')) == NULL ||
                    (*ptr++ = 0, sscanf(buf, "%s", name) != 1)));
                if (sscanf(ptr, "%ld%ld%ld%ld%ld%ld%ld%*d%ld%ld%ld%ld%ld%ld%ld",
                           &rx_bytes, &rx_packets, &rx_errs, &rx_drops,
                           &rx_fifo, &rx_frame, &rx_multi,
                           &tx_bytes, &tx_packets, &tx_errs, &tx_drops,
                           &tx_fifo, &tx_colls, &tx_carrier) != 14)
                        continue;
                if (strcmp(ifname->cstr(), name))
                        continue;
                found = true;
                break;
        }
        fclose(fp);
        CHECK(found);
        return rx_bytes;
    }

    std::map<str, ptr<VMM> > monitored_vmms_;
    ptr<asrv> obs_srv_;
};

int
main(int argc, char** argv) {
    bool daemonize;
    std::string log_out;
    async_init();
    if (argc == 2) {
        Config::LoadConfig(argv[1]);
    }
    Config::GetFromConfig("daemonize", &daemonize, true);
    Config::GetFromConfig("logfile", &log_out,
                          static_cast<std::string>("/jffs/ntfa.log"));
    ref<VMMEnforcer> e = New refcounted<VMMEnforcer>;
    e->Init();
    e->Prioritize();
    if (daemonize) {
        e->Daemonize(log_out.c_str());
    }
    e->Run();
    return EXIT_FAILURE;
}
