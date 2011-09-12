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
#include "os_spy/os_enforcer.h"
#include "vmm_spy/vmm_enforcer.h"

#include "enforcer/spy_prot.h"

#include "async.h"
#include <libvirt/libvirt.h>

#include "common.h"
#include "config.h"
#include "enforcer/enforcer.h"
#include "os_spy/os_worker.h"
#include "obs_prot.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

static const uint32_t kHostnameLength = 64;
static const uint32_t kDefaultGenWait = 5;

class OSEnforcer : public virtual Enforcer {
  public:
    virtual ~OSEnforcer() {}

    virtual void Init() {
        Config::GetFromConfig("worker_path", &_worker_path_,
                              static_cast<std::string>
                              ("/home/ntfa/ntfa/code/falcon/os_enforcer/os_worker"));
        worker_path_ = _worker_path_.c_str();

        std::string router_hostname;
        Config::GetFromConfig("router_hostname", &router_hostname,
                              static_cast<std::string>("router"));
        vmm_connection_ = NULL;
        vmm_connection_ = virConnectOpen(kHypervisorPath);
        CHECK(vmm_connection_);

        logfile_name_ = "/dev/null";
        // Get router's address
        addrinfo hints;
        addrinfo* aret = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        CHECK(0 == getaddrinfo(router_hostname.c_str(), NULL, &hints, &aret));

        // Create router client
        int cfd = inetsocket(SOCK_DGRAM, 0, 0);
        CHECK(cfd >= 0);
        make_async(cfd);
        close_on_exec(cfd);
        struct sockaddr_in iaddr;
        memset(&iaddr, 0, sizeof(iaddr));
        iaddr.sin_family = AF_INET;
        iaddr.sin_addr.s_addr = reinterpret_cast<sockaddr_in *>(
                                aret->ai_addr)->sin_addr.s_addr;
        iaddr.sin_port = htons(kFalconPort);
        ref<aclnt> router_clnt = 
          aclnt::alloc(axprt_dgram::alloc(cfd), spy_prog_1,
                       reinterpret_cast<sockaddr *>(&iaddr),
                                            callbase_alloc<rpccb_unreliable>);

        char hostname[kHostnameLength];
        gethostname(hostname, kHostnameLength);
        spy_get_gen_arg arg;
        arg.target.handle = str(hostname);

        ref<spy_res> res = 
          New refcounted<spy_res>();

        router_clnt->call(SPY_GET_GEN, &arg, res, wrap(mkref(this),
                          &OSEnforcer::SetGeneration, res));

        // cleanup
        freeaddrinfo(aret);
        return;
    }

    void SetGeneration(ref<spy_res> res, clnt_stat st) {
        CHECK(st == RPC_SUCCESS);
        gen_vec_ = res->target.generation;
    }

    virtual void StartMonitoring(const ref<const str> handle) {
        LOG("START MONITORING %s", handle->cstr());
        // Flow for this taken from paxos made practical source
        // 0. Check that domain is legit
        virDomainPtr dom = virDomainLookupByName(vmm_connection_,
                                                 handle->cstr());
        CHECK(dom);
        // 1. Create a pipe
        int pipefds[2];
        CHECK(0 == pipe(pipefds));
        // 2. spawn worker
        int devnull = open("/dev/null", O_RDONLY);
        CHECK(devnull >= 0);

        const char* av[] = {worker_path_.cstr(), handle->cstr(), 0};
        const char* lethal_av[] = {worker_path_.cstr(), handle->cstr(), "lethal", 0};

        make_async(pipefds[0]);
        close_on_exec(pipefds[0]);
        // Share a stderr
        pid_t pid;
        if (Killable(handle)) {
            pid = spawn(worker_path_, lethal_av, devnull, pipefds[1],
                          fileno(stderr));
        } else {
            pid = spawn(worker_path_, av, devnull, pipefds[1],
                          fileno(stderr));
        }
        CHECK(pid >= 0);
        close(devnull);
        close(pipefds[1]);
        // 3. Create callback to process worker
        fdcb(pipefds[0], selread, wrap(mkref(this), &OSEnforcer::HandleWorker,
                                       handle, pipefds[0]));
        // 4. Add domains to active domains
        active_domains_[*handle] = dom;
        return;
    }

    virtual void StopMonitoring(const ref<const str> handle) {
        virDomainPtr target = active_domains_[*handle];
        LOG("STOP MONITORING %s", handle->cstr());
        if (target) virDomainFree(target);
        active_domains_[*handle] = NULL;
        return;
    }

    virtual bool InvalidTarget(const ref<const str> handle) {
        if (!active_domains_[*handle]) {
            virDomainPtr target = virDomainLookupByName(vmm_connection_,
                                                        handle->cstr());
            if (target == NULL) return true;
            virDomainFree(target);
        }
        return false;
    }

    virtual void Kill(const ref<const str> handle) {
        virDomainPtr target = active_domains_[*handle];
        CHECK(target);
        // The following check is probably incorrect, but is good for
        // detecting unexpected conditions.
        if (virDomainIsActive(target)) {
            CHECK(0 == virDomainDestroy(target));
        }
        ObserveDown(handle, 0, true, true);
        return;
    }

    virtual void UpdateGenerations(const ref<const str> handle) {
        virDomainPtr target = active_domains_[*handle];
        uint32_t gen = 0;
        if (target == NULL) {
            target = virDomainLookupByName(vmm_connection_, handle->cstr());
            if (target == NULL) return;
            gen = virDomainGetNtfaGeneration(target);
            virDomainFree(target);
            FastForwardGeneration(handle, gen);
        }
        return;
    }

  private:
    void HandleWorker(const ref<const str> handle, int fd) {
        // If we aren't monitoring it, we don't care. Close the
        // pipe to kill the process next time it tries to write.
        if (active_domains_[*handle] == NULL) {
            close(fd);
            fdcb(fd, selread, 0);
            return;
        }
        worker_state state;
        size_t rsize = read(fd, &state, sizeof(state));
        CHECK(rsize == sizeof(state));
        if (state != VM_OK) LOG("Vm not OK: %s %d", handle->cstr(), state);
        switch (state) {
            case VM_OK: {
                ObserveUp(handle);
                return;
            }
            case VM_ERROR:
            case VM_NEEDS_KILL:
                if (Killable(handle)) {
                    Kill(handle);
                } else {
                    ObserveDown(handle, state, false, true);
                }
                break;
            case VM_KILLED:
                ObserveDown(handle, state, false, true);
                break;
            case VM_DEAD:
                ObserveDown(handle, state, false, false);
                break;
        }
        close(fd);
        fdcb(fd, selread, 0);
        active_domains_[*handle] = NULL;
        return;
    }

    std::map<str, virDomainPtr> active_domains_;
    virConnectPtr vmm_connection_;
    std::string _worker_path_;
    str worker_path_;

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
                          static_cast<std::string>("/dev/shm/ntfa.log"));
    ref<OSEnforcer> e = New refcounted<OSEnforcer>;
    if (daemonize) {
        e->Daemonize(log_out.c_str());
    }
    e->Init();
    e->Prioritize();
    // Reserve a bunch of memory on the heap.
    e->Run();
    return EXIT_FAILURE;
}
