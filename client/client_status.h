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
#ifndef _NTFA_FALCON_CLIENT_CLIENT_STATUS_H_
#define _NTFA_FALCON_CLIENT_CLIENT_STATUS_H_
const uint32_t DOWN_FROM_REMOTE = 0;
const uint32_t KILLED_BY_REMOTE = 1;
const uint32_t REMOTE_WOULD_KILL = 2;
const uint32_t REMOTE_ERROR = 3;
const uint32_t UNKNOWN_ERROR = 4;
const uint32_t FALCON_E2E_TIMEOUT = 5;
const uint32_t KILL_LONG_DEAD = 6; 
const uint32_t REGISTRATION_ERROR = 7;
const uint32_t SIGN_OF_LIFE = 8;
#endif  //_NTFA_FALCON_CLIENT_CLIENT_STATUS_H_
