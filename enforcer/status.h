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
#ifndef _NTFA_ENFORCER_STATUS_H_
#define _NTFA_ENFORCER_STATUS_H_

enum spy_status {
    FALCON_SUCCESS = 0,
    FALCON_UNKNOWN_TARGET = 1,
    FALCON_BAD_GEN_VEC = 2,
    FALCON_FUTURE_GEN = 3,
    FALCON_LONG_DEAD = 4,
    FALCON_REGISTER_ACK = 5,
    FALCON_CANCEL_ACK = 6,
    FALCON_CANCEL_ERROR = 7,
    FALCON_KILL_ACK = 8,
    FALCON_GEN_RESP = 9,
    FALCON_UP = 10,
    FALCON_DOWN = 11,
    FALCON_UNKNOWN_ERROR = 12
};
#endif  // _NTFA_ENFORCER_STATUS_H_
