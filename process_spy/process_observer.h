#ifndef _NTFA_SPIES_APPLICATION_SPY_H_
#define _NTFA_SPIES_APPLICATION_SPY_H_

#include "common.h"
#include "process_spy/obs_prot.h"

// This file defines the spy interface.
typedef uint32_t (*spyfunc_f)(void);

void SetHandle(const char *handle);
void SetSpy(spyfunc_f, const char *handle);
void SetSpy(spyfunc_f, const char *handle, delay_type delay, uint32_t delay_ms);

#endif  // _NTFA_SPIES_APPLICATION_SPY_H_
