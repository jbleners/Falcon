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
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

int
main () {
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

  int proc_fd = open("/proc/ntfa", O_WRONLY);
  // Increment
  for (;;) {
    char crap;
    usleep(1000);
    CHECK(1 == write(proc_fd, &crap, 1));
  }

}
