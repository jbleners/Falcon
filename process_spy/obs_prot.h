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
