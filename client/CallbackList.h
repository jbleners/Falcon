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
