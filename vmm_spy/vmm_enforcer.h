#ifndef _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_
#define _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_
#include <stdint.h>
enum vmm_enforcer_stat {
    VMM_TIMEDOUT,
    VMM_CANCELED
};

const uint16_t kDefaultVMMProbePort = 2001;
#endif  // _NTFA_VMM_ENFORCER_VMM_ENFORCER_H_
