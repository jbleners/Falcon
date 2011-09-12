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
