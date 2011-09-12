#ifndef _NTFA_PROCESS_ENFORCER_PARSE_PROC_
#define _NTFA_PROCESS_ENFORCER_PARSE_PROC_
#include <sys/types.h>
#include <list>
#include "common.h"


// Struct containing the information from /proc/pid/stat
typedef struct proc_info {
    pid_t               pid;
    char                cmd[24];  // Supposedly only 16 + 2 char (18 total)
    char                state;
    pid_t               ppid;
    pid_t               pgrp;
    int                 session;
    int                 tty_nr;
    int                 tpgid;
    unsigned int        flags;
    long unsigned int   minflt;
    long unsigned int   cminflt;
    long unsigned int   majflt;
    long unsigned int   cmajflt;
    long unsigned int   utime;
    long unsigned int   stime;
    long int            cutime;
    long int            cstime;
    long int            priority;
    long int            nice;
    long int            num_threads;
    long int            itrealvalue;
    long long unsigned int starttime;
    long unsigned int   vsize;
    long int            rss;
    long unsigned int   rsslim;
    long unsigned int   startcode;
    long unsigned int   endcode;
    long unsigned int   startstack;
    long unsigned int   kstkesp;
    long unsigned int   kstkeip;
    // BEGIN OBSOLETE
    long unsigned int   signal;
    long unsigned int   blocked;
    long unsigned int   sigignore;
    long unsigned int   sigcatch;
    // END OBSOLETE
    long unsigned int   wchan;
    long unsigned int   nswap;
    long unsigned int   cnswap;
    int                 exit_signal;
    int                 processor;
    unsigned int        rt_priority;
    unsigned int        policy;
    long long unsigned int delayacct_blkio_ticks;
    long unsigned int   guest_time;
    long int            cguest_time;
}*proc_info_p, proc_info_s;

proc_info_p GetProcByPid(int pid);
#endif  // _NTFA_PROCESS_ENFORCER_PARSE_PROC_
