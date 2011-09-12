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
#include "CallbackList.h"

#include "common.h"

CallbackList::CallbackList() {
    pthread_mutex_init(&cb_list_lock_, NULL);
}

CallbackList::~CallbackList() {
    pthread_mutex_destroy(&cb_list_lock_);
}

void
CallbackList::Add(FalconCallbackPtr cb) {
    pthread_mutex_lock(&cb_list_lock_);
    cb_list_.push_back(cb);
    pthread_mutex_unlock(&cb_list_lock_);
    return;
}

void
CallbackList::Run(const LayerId& lid, uint32_t falcon_status,
                  uint32_t remote_status) {
    pthread_mutex_lock(&cb_list_lock_);
    std::list<FalconCallbackPtr> tmp_list = cb_list_;
    cb_list_.clear();
    pthread_mutex_unlock(&cb_list_lock_);
    std::list<FalconCallbackPtr>::iterator it;
    for (it = tmp_list.begin(); it != tmp_list.end(); ++it) {
        (**it)(lid, falcon_status, remote_status);
    }
    return;
}
