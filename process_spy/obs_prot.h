#ifndef _NTFA_PROCESS_ENFORCER_OBS_PROT_H_
#define _NTFA_PROCESS_ENFORCER_OBS_PROT_H_

typedef enum delay_type {
    DELAY_REALTIME = 0,
    DELAY_CPUTIME = 1
} delay_time;

typedef enum enforcer_state {
    SOCKET_ERROR,
    ENF_TIMEDOUT,
    ENF_CPU_TIMEOUT
} enforcer_state;

const size_t kHandleSize = 32;

struct process_observer_handshake {
        char        handle[kHandleSize];
        uint32_t    pid;
        uint32_t    delay_ms;
        uint8_t     delay;
} __attribute__((__packed__));

struct process_observer_probe {
    uint64_t _garbage;
} __attribute__((__packed__));

struct process_observer_reply {
    uint32_t state;
} __attribute__((__packed__));

#define falcon_process_enforcer_socket  "/tmp/falcon"
#define PROC_OBS_ALIVE 0
#define PROC_OBS_DEAD 1

#endif  // _NTFA_PROCESS_ENFORCER_OBS_PROT_H_
