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
