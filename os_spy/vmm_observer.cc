#include "async.h"
#include "arpc.h"

#include <arpa/inet.h>

#include "common.h"
#include "config.h"
#include "obs_prot.h"
#include "vmm_spy/vmm_enforcer.h"

static const uint32_t kHostnameLength = 64;

void
ObserverDispatch(svccb *sbp) {
  switch (sbp->proc()) {
    case VMM_OBS_NULL: {
      sbp->reply(0);
      return;
    }
    case VMM_OBS_PROBE: {
      obs_probe_msg *argp =
                      sbp->Xtmpl getarg<obs_probe_msg>();
      argp->counter++;
      sbp->reply(argp);
      break;
    }
    case VMM_OBS_REGISTER:
    case VMM_OBS_CANCEL:
    default:
      sbp->ignore();
      return;
  }
  return;
}

int
main (int argc, char** argv) {
  if (argc == 2) {
    Config::LoadConfig(argv[1]);
  }
  std::string router_hostname;
  Config::GetFromConfig("router_hostname", &router_hostname,
                        static_cast<std::string>("router"));
  // Get router's address
  addrinfo hints;
  addrinfo* aret = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  while(0 != getaddrinfo(router_hostname.c_str(), NULL, &hints, &aret)) {
        printf("Cannot reach %s, sleeping and retrying.\n",
               router_hostname.c_str());
        sleep(5);
  }

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
  iaddr.sin_port = htons(kDefaultVMMProbePort);
  ref<aclnt> obs_clnt = aclnt::alloc(axprt_dgram::alloc(cfd), vmm_obs_prog_1,
                                      reinterpret_cast<sockaddr *>(&iaddr),
                                      callbase_alloc<rpccb_unreliable>);

  char hostname[kHostnameLength];
  gethostname(hostname, kHostnameLength);
  obs_register_arg arg;
  memset(&arg, 0, sizeof(arg));
  arg.handle = str(hostname);

  ref<obs_register_response> res = New refcounted<obs_register_response>();
  while(RPC_SUCCESS != obs_clnt->scall(VMM_OBS_REGISTER, &arg, res)) {
        printf("Cannot reach %s, sleeping and retrying.\n",
               router_hostname.c_str());
        sleep(5);
  }

  // Get away from the parent
  pid_t pid, sid;
  if (getppid() == 1) return -1;
  pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);
  umask(0);
  sid = setsid();
  if (sid < 0) exit(EXIT_FAILURE);
  if (chdir("/") < 0) exit(EXIT_FAILURE);

  // Set the new fds
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "a", stdout);
  freopen("/dev/null", "a", stderr);
  CHECK(0 == fcntl(0, F_SETFL, O_NONBLOCK));
  CHECK(0 == fcntl(1, F_SETFL, O_NONBLOCK));
  CHECK(0 == fcntl(2, F_SETFL, O_NONBLOCK));

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

  // start server
  int fd = inetsocket(SOCK_DGRAM, kDefaultVMMProbePort, INADDR_ANY);
  CHECK(fd >= 0);
  make_async(fd);
  close_on_exec(fd);
  ref<asrv> obs_srv = asrv::alloc(axprt_dgram::alloc(fd), vmm_obs_prog_1);
  obs_srv->setcb(wrap(ObserverDispatch));

  amain();
  return 1;
}
