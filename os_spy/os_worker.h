#ifndef _NTFA_OS_ENFORCER_OS_WORKER_H_
#define _NTFA_OS_ENFORCER_OS_WORKER_H_
enum worker_state {
    VM_OK,
    VM_ERROR,
    VM_NEEDS_KILL,
    VM_KILLED,
    VM_DEAD
};

const char* kHypervisorPath = "qemu:///system";
#endif  // _NTFA_OS_ENFORCER_OS_WORKER_H_
