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
#ifndef _NTFA_FALCON_CLIENT_CALLBACKLIST_H_
#define _NTFA_FALCON_CLIENT_CALLBACKLIST_H_

#include <pthread.h>
#include <stdint.h>

#include <list>

#include "FalconCallback.h"

class CallbackList {
  public:
    CallbackList();

    ~CallbackList();
    // Adds a CallbackPtr to the list
    void Add(FalconCallbackPtr cb);

    // Runs callbacks with the supplied arguments.
    // Note: Calling two lists with the same callback on them is OK in the
    // following sense: A callback will be called EXACTLY once, but there is
    // no guarantee as to which callback list will "win"
    void Run(const LayerId& lid, uint32_t falcon_status,
             uint32_t remote_status);

  private:
    // Synchronization and state.
    pthread_mutex_t cb_list_lock_;
    std::list<FalconCallbackPtr> cb_list_;
};

#endif  // _NTFA_FALCON_CLIENT_CALLBACKLIST_H_
