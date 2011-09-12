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
#include "FalconCallback.h"
#include "FalconClient.h"
#include "common.h"

struct cb_package {
    cb_package(falcon_callback_fn cb_,
               const LayerIdList& id_list_,
               void* cd,
               uint32_t falcon_st,
               uint32_t remote_st,
               int idx) :
        cb(cb_), id_list(id_list_), client_data(cd), falcon_status(falcon_st),
        remote_status(remote_st), index(idx) {}
    falcon_callback_fn          cb;
    const LayerIdList           id_list;
    void*                       client_data;
    uint32_t                    falcon_status;
    uint32_t                    remote_status;
    int                         index;
};

namespace {
// This is the thread function that actually runs the callback function.
void*
cb_run_thread(void *arg) {
    cb_package* pack = static_cast<cb_package*>(arg);
    pack->cb(pack->id_list, pack->client_data,
             (pack->index << 16) | pack->falcon_status, pack->remote_status);
    pthread_detach(pthread_self());
    delete pack;
    return NULL;
}
}

FalconCallback::~FalconCallback() {
    pthread_mutex_destroy(&cb_lock_);
}

void
FalconCallback::Deactivate() {
    pthread_mutex_lock(&cb_lock_);
    f_ = NULL;
    pthread_mutex_unlock(&cb_lock_);
}

bool
FalconCallback::Reactivate(falcon_callback_fn f, void* cd) {
    bool ret = false;
    pthread_mutex_lock(&cb_lock_);
    f_ = f;
    if (cd) {
        cd_ = cd;
    }
    if (run_final_) {
        cb_package* pack = new cb_package(f_, h_, cd_, deferred_falcon_status_,
                                          deferred_remote_status_,
                                          deferred_index_);
        pthread_t tmp;
        CHECK(0 == pthread_create(&tmp, NULL, cb_run_thread,
                                  reinterpret_cast<void*>(pack)));
        ret = true;
    }
    pthread_mutex_unlock(&cb_lock_);
    return ret;
}

void
FalconCallback::SetData(void* new_data) {
    pthread_mutex_lock(&cb_lock_);
    cd_ = new_data;
    pthread_mutex_unlock(&cb_lock_);
}

bool
FalconCallback::HasRunFinal() {
  bool ret;
  pthread_mutex_lock(&cb_lock_);
  ret = run_final_;
  pthread_mutex_unlock(&cb_lock_);
  return ret;
}

void
FalconCallback::operator() (const LayerId& lid, uint32_t falcon_status,
                            uint32_t remote_status) {
    pthread_mutex_lock(&cb_lock_);
    size_t i;
    if (falcon_status == SIGN_OF_LIFE) {
        i = h_.size() - 1;
    } else {
        for (i = 0; i < h_.size(); ++i) {
            if (h_[i] == lid) break;
        }
    }
    if (f_) {
        cb_package* pack = new cb_package(f_, h_, cd_, falcon_status,
                                          remote_status, i);
        pthread_t tmp;
        CHECK(0 == pthread_create(&tmp, NULL, cb_run_thread,
                                  reinterpret_cast<void*>(pack)));
    } else if (!run_final_) {
        deferred_falcon_status_ = falcon_status;
        deferred_remote_status_ = remote_status;
        deferred_index_ = i;
    }
    if (!repeatable_ || falcon_status != SIGN_OF_LIFE) {
        f_ = NULL;
        run_final_ = true;
    }
    pthread_mutex_unlock(&cb_lock_);
}
