#include "process_spy/parse_proc.h"

#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <map>

namespace {
const int kBufferSize = 256;
const int kMaxLineSize = 1024;
const int kNumProcStatFields = 44;
// Format of /proc/pid/stat
const char *kStatLineFmt = "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld\n";

// Processes stat_line from /proc/pid/stat
// Fills info with the contents.
void
ParseStat(const char *stat_line, proc_info_p info) {
    int scanned = sscanf(stat_line, kStatLineFmt,
            &(info->pid), &(info->cmd[0]), &(info->state), &(info->ppid),
            &(info->pgrp), &(info->session), &(info->tty_nr), &(info->tpgid),
            &(info->flags), &(info->minflt), &(info->cminflt), &(info->majflt),
            &(info->cmajflt), &(info->utime), &(info->stime), &(info->cutime),
            &(info->cstime), &(info->priority), &(info->nice),
            &(info->num_threads), &(info->itrealvalue), &(info->starttime),
            &(info->vsize), &(info->rss), &(info->rsslim), &(info->startcode),
            &(info->endcode), &(info->startstack), &(info->kstkesp),
            &(info->kstkeip), &(info->signal), &(info->blocked),
            &(info->sigignore), &(info->sigcatch), &(info->wchan),
            &(info->nswap), &(info->cnswap), &(info->exit_signal),
            &(info->processor), &(info->rt_priority), &(info->policy),
            &(info->delayacct_blkio_ticks), &(info->guest_time),
            &(info->cguest_time));
    assert(scanned == kNumProcStatFields);
    return;
}
}  // end anonymous namespace

proc_info_p
GetProcByPid(int pid) {
    char path_buffer[kBufferSize];
    char line_buffer[kMaxLineSize];
    snprintf(path_buffer, kBufferSize, "/proc/%d/stat", pid);
    FILE *proc_file = fopen(path_buffer, "r");
    if (NULL == proc_file) {
        errno = 0;  // Acknowledged the file not found error.
        return NULL;
    }
    assert(fgets(line_buffer, kMaxLineSize, proc_file));
    proc_info_p stat = (proc_info_p) malloc(sizeof(*stat));
    ParseStat(line_buffer, stat);
    int ret = fclose(proc_file);
    assert(!ret);
    return stat;
}
