#include "process_spy/process_enforcer.h"

#include "async.h"

#include "common.h"
#include "config.h"
#include "enforcer/enforcer.h"
#include "process_spy/obs_prot.h"
#include "process_spy/parse_proc.h"

namespace {
struct Process {
    public:
        Process(const process_observer_handshake* h, int cfd) {
            handle = New refcounted<const str>(static_cast<const char*>
                                               (h->handle));
            pid = h->pid;
            delay = static_cast<delay_type>(h->delay);
            delay_ms = h->delay_ms;
            fd = cfd;
            cb = NULL;
            state = 0;
            active = false;
        }

        ptr<const str> handle;
        pid_t pid;
        delay_type delay;
        uint32_t delay_ms;
        int fd;
        timecb_t* cb;
        uint32_t state;
        bool active;
};

// Although the process has control over its timeout, the enforcer has control
// over the polling frequency.
uint32_t confirm_wait_ns;
uint32_t poll_freq_ns;
int32_t cpu_time_wait_multiplier;
}

class ProcessEnforcer : public virtual Enforcer {
  public:
    virtual ~ProcessEnforcer() {}

    virtual void Init() {
        // Set up the UNIX socket
        int unix_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        CHECK(unix_socket > 0);
        int tmp = 1;
        CHECK(0 == setsockopt(unix_socket, SOL_SOCKET, SO_REUSEADDR, &tmp,
                              sizeof(tmp)));
        struct sockaddr_un handler_addr;
        memset(&handler_addr, 0, sizeof(handler_addr));
        handler_addr.sun_family = AF_UNIX;
#define UNIX_MAX_PATH 108  // from the man pages
        strncpy(handler_addr.sun_path, falcon_process_enforcer_socket,
                UNIX_MAX_PATH);
        CHECK(0 == bind(unix_socket, (struct sockaddr *) &handler_addr,
                        sizeof(handler_addr)));
        chmod(falcon_process_enforcer_socket, 722);
        CHECK(0 == listen(unix_socket, 10));
        fdcb(unix_socket, selread, wrap(mkref(this),
             &ProcessEnforcer::AcceptProcess, unix_socket));

        // Obligatory enforcer business
        logfile_name_ = "/dev/shm/falcon.log";

        // Initialize our generations
        SetGenVec();

        // Start the incrementer
//        int proc_fd = open("/proc/ntfa", O_WRONLY);
//        Incrementer(proc_fd);
//        
//        JBL: could start here, but will start in script start_handlerd.sh
//        std::string incrementer_path;
//        Config::GetFromConfig("incrementer_path", &incrementer_path,
//                              static_cast<std::string>(
//                              "/home/ntfa/ntfa/code/falcon/process_enforcer/incrementer"));
//        int devnull = open("/dev/null", O_RDWR);
//        const char* av[] = {incrementer_path.c_str(), 0};
//        pid_t pid = spawn(incrementer_path.c_str(), av, devnull, devnull,
//                          fileno(stderr));
//        close(devnull);
//        CHECK(pid >= 0);

        // Get a system variable
        clck_tick_ = 1.0 * sysconf(_SC_CLK_TCK);

        uint32_t confirm_wait_ms, poll_freq_ms;
        Config::GetFromConfig("confirm_wait_ms", &confirm_wait_ms,
                              (uint32_t) 5);
        Config::GetFromConfig("poll_freq_ms", &poll_freq_ms, (uint32_t) 100);
        Config::GetFromConfig("cpu_time_wait_multiplier",
                              &cpu_time_wait_multiplier, (int32_t) 1);

        confirm_wait_ns = confirm_wait_ms * kMillisecondsToNanoseconds;
        poll_freq_ns = poll_freq_ms * kMillisecondsToNanoseconds;
        return;
    }

    virtual void StartMonitoring(const ref<const str> handle) {
        LOG("START MONITORING %s", handle->cstr());
        Process* p = monitored_[*handle];
        p->active = true;
        ProbeProcess(p);
        return;
    }

    virtual void StopMonitoring(const ref<const str> handle) {
        LOG("STOP MONITORING %s", handle->cstr());
        Process* p = monitored_[*handle];
        if (p->cb) timecb_remove(p->cb);
        p->cb = NULL;
        p->active = false;
        return;
    }

    virtual bool InvalidTarget(const ref<const str> handle) {
        return (monitored_.count(*handle) == 0);
    }

    virtual void Kill(const ref<const str> handle) {
        Process* p = monitored_[*handle];
        CHECK(p);
        if (!p->active) return;
        LOG("killing %s", handle->cstr());
        if (!check_process_table(p->pid)) {
            ObserveDown(handle, p->state, false, false);
        } else if (Killable(handle)) {
            int ret = kill(p->pid, SIGKILL);
            // The following covers the race condition in which the process goes
            // away between our first check and our kill attempt.
            if (ret != 0 && errno == ESRCH) {
                ObserveDown(handle, p->state, false, false);
                return;
            }
            delaycb(0, confirm_wait_ns, wrap(mkref(this),
                    &ProcessEnforcer::ConfirmDeath, handle, true, true));
        } else {
          ObserveDown(handle, p->state, false, true);
        }
    }

    virtual void UpdateGenerations(const ref<const str> handle) {
        // NO-OP: Generations are tracked by the enforcer core
        return;
    }

  private:
    std::map<str, timecb_t*> monitored_cb_;
    std::map<str, Process*> monitored_;
    double clck_tick_;

//    void Incrementer(int fd) {
//        char crap;
//        CHECK(1 == write(fd, &crap, 1));
//        delaycb(0, kMillisecondsToNanoseconds, wrap(mkref(this),
//                &ProcessEnforcer::Incrementer, fd));
//    }

    void SetGenVec() {
        static const uint32_t kBufferSize = 256;
        uint32_t domain_generation;
        uint32_t hypervisor_generation;
        int gen_fd = open("/mnt/gen/hypervisor", O_RDONLY);
        CHECK(gen_fd >= 0);
        CHECK(sizeof(uint32_t) == read(gen_fd, &(hypervisor_generation),
                                       sizeof(uint32_t)));
        close(gen_fd);

        char buf[kBufferSize];
        char hostname[kBufferSize];
        CHECK(0 == gethostname(hostname, kBufferSize));
        snprintf(buf, kBufferSize, "/mnt/gen/%s", hostname);
        gen_fd = open(buf, O_RDONLY);
        CHECK(gen_fd >= 0);
        CHECK(sizeof(uint32_t) == read(gen_fd, &(domain_generation),
                                       sizeof(uint32_t)));
        close(gen_fd);
        gen_vec_.push_back(domain_generation);
        gen_vec_.push_back(hypervisor_generation);
    }

    static inline bool check_process_table(pid_t pid) {
        return (0 == kill(pid, 0));
    }

    void ConfirmDeath(const ref<const str> handle, bool killed,
                      bool would_kill) {
        Process* p = monitored_[*handle];
        CHECK(p);
        if (!check_process_table(p->pid) || !Killable(handle)) {
            ObserveDown(handle, p->state, killed, would_kill);
        } else {
            delaycb(0, confirm_wait_ns, wrap(mkref(this),
                    &ProcessEnforcer::ConfirmDeath, handle, killed,
                    would_kill));
        }
    }

    void ProcessResponse(Process* p) {
        process_observer_reply reply;
        memset(&reply, 0, sizeof(reply));
        if (p->cb) timecb_remove(p->cb);
        p->cb = NULL;
        if (sizeof(reply) ==
                recv(p->fd, &reply, sizeof(reply), 0)) {
            if (!p->active) {
                fdcb(p->fd, selread, 0);
                return;
            }
            if (reply.state == PROC_OBS_ALIVE) {
                ObserveUp(p->handle);
                delaycb(0, poll_freq_ns, wrap(mkref(this),
                        &ProcessEnforcer::ProbeProcess, p));
                return;
            } else {
                p->state = reply.state;
                Kill(p->handle);
                return;
            }
        }
        close(p->fd);
        fdcb(p->fd, selread, 0);
        Kill(p->handle);
    }

    void ProcessTimeout(Process* p) {
        LOG("Response timeout");
        p->state = ENF_TIMEDOUT;
        close(p->fd);
        fdcb(p->fd, selread, 0);
        p->cb = NULL;
        Kill(p->handle);
    }

    uint32_t GetCPUTime(Process* p) {
        uint32_t ret;
        proc_info_p pi = GetProcByPid(p->pid);
        ret = pi->utime + pi->stime;
        free(pi);
        return ret;
    }

    void CheckCPUTime(Process* p, uint32_t start_time) {
        if (((GetCPUTime(p) - start_time)/clck_tick_) >
            (kMillisecondsToSeconds * p->delay_ms)) {
            p->state = ENF_CPU_TIMEOUT;
            close(p->fd);
            fdcb(p->fd, selread, 0);
            p->cb = NULL;
            Kill(p->handle);
        } else {
            int32_t delay_ms = p->delay_ms * cpu_time_wait_multiplier;
            int32_t delay_s = delay_ms / kSecondsToMilliseconds;
            int32_t delay_ns = (delay_ms%kSecondsToMilliseconds)
                               * kMillisecondsToNanoseconds;

            p->cb = delaycb(delay_s, delay_ns, wrap(mkref(this),
                            &ProcessEnforcer::CheckCPUTime, p, start_time));
        }
    }

    void ProbeProcess(Process* p) {
        if (check_process_table(p->pid)) {
            process_observer_probe probe;
            memset(&probe, 0, sizeof(probe));
            if (sizeof(probe) ==
                    send(p->fd, &probe, sizeof(probe), 0)) {
                // Construct the delay
                int32_t delay_ms = (p->delay == DELAY_REALTIME) ?
                                   1 :
                                   cpu_time_wait_multiplier;
                delay_ms *= p->delay_ms;
                int32_t delay_s = p->delay_ms / kSecondsToMilliseconds;
                int32_t delay_ns = (p->delay_ms%kSecondsToMilliseconds)
                                   * kMillisecondsToNanoseconds;

                if (p->delay == DELAY_REALTIME) {
                    p->cb = delaycb(delay_s, delay_ns, wrap(mkref(this),
                                    &ProcessEnforcer::ProcessTimeout, p));
                } else {
                    uint32_t start_time = GetCPUTime(p);
                    p->cb = delaycb(delay_s, delay_ns, wrap(mkref(this),
                                    &ProcessEnforcer::CheckCPUTime, p,
                                    start_time));
                }
                fdcb(p->fd, selread, wrap(mkref(this),
                     &ProcessEnforcer::ProcessResponse, p));
                return;
            }
        }
        LOG("Killing %s", p->handle->cstr());
        close(p->fd);
        fdcb(p->fd, selread, 0);
        Kill(p->handle);
    }

    void ClientAcceptor(int fd) {
        process_observer_handshake handshake;
        // TODO(leners): Can short messages be broken up on unix pipes?
        if (sizeof(handshake) ==
                recv(fd, &handshake, sizeof(handshake), 0)) {
            if (monitored_[handshake.handle]) {
                // What happens next is not entirely correct. The process that
                // we are monitoring could have died and another process took
                // it's pid. There is a way to check for this, assuming the
                // handle is always part of the procfile, but we're not doing
                // this right now.
                Process* current = monitored_[handshake.handle];
                if (!current->active &&
                    !check_process_table(current->pid)) {
                    LOG("replacing dead process %s", handshake.handle);
                    delete current;
                    Process* p = New Process(&handshake, fd);
                    LOG("%d is the delay_ms", p->delay_ms);
                    monitored_[*(p->handle)] = p;
                } else {
                    LOG("process %s exists and is active", handshake.handle);
                    close(fd);
                    fdcb(fd, selread, 0);
                }
            } else {
                Process* p = New Process(&handshake, fd);
                LOG("%d is the delay_ms, %s is the delay type", p->delay_ms,
                    (p->delay == DELAY_REALTIME) ? "real time" : "cpu time");
                monitored_[*(p->handle)] = p;
            }
        } else {
            close(fd);
            fdcb(fd, selread, 0);
        }
    }

    void AcceptProcess(int fd) {
        int newfd = accept(fd, NULL, NULL);
        CHECK(newfd >= 0);
        fdcb(newfd, selread, wrap(mkref(this),
             &ProcessEnforcer::ClientAcceptor, newfd));
    }
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
                          static_cast<std::string>("/tmp/ntfa.log"));
    ref<ProcessEnforcer> e = New refcounted<ProcessEnforcer>();
    e->Init();
    e->Prioritize();
    if (daemonize) {
        e->Daemonize(log_out.c_str());
    }
    e->Run();
    return EXIT_FAILURE;
}
