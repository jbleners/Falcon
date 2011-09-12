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
#ifndef _NTFA_FALCON_CLIENT_FALCONLAYER_H_
#define _NTFA_FALCON_CLIENT_FALCONLAYER_H_

#include <boost/smart_ptr.hpp>
#include <pthread.h>

#include <map>
#include <list>

#include "spy_prot.h"
#include "CallbackList.h"
#include "FalconCallback.h"
#include "Generation.h"


class FalconLayer;
class Watchdog;
class FalconClient;
typedef boost::shared_ptr<FalconLayer> FalconLayerPtr;
typedef boost::weak_ptr<FalconLayer> FalconParentPtr;

class FalconLayer {
    public:
        FalconLayer(const LayerId& h, const Generation& g, bool killable,
                    FalconParentPtr parent, client_addr_t& addr,
                    uint32_t timeout, bool has_watchdog = false,
                    bool is_base = false, sockaddr_in* my_addr = NULL);
        ~FalconLayer();

        // LayerId Spy callbacks
        void DoChildUp(const LayerId& child, Generation gen);
        void DoChildDown(const LayerId& child, Generation gen,
                         uint32_t falcon_status, uint32_t remote_status);

        // LayerId client/watchdog requests
        bool            CancelChild(const LayerId& child);
        int             KillChild(const LayerId& child);
        FalconLayerPtr  AddChild(const LayerId& child, bool killable,
                                 client_addr_t& addr, int32_t timeout,
                                 bool leaf_layer, FalconParentPtr parent,
                                 FalconCallbackPtr first_cb);

        // e2etimer functions
        void StartTimer(FalconCallbackPtr cb, int timeout);
        void StopTimer();
        void Cancel();

        // Add a callback to this layer
        void AddCallback(FalconCallbackPtr cb);

        void AggravateWatchdog();
        friend class Watchdog;
        friend class FalconClient;
    private:
        // For the watchdog
        FalconLayerPtr  GetParent() const;
        const LayerId&   GetLayerId() const;

        // For the kill interface
        FalconLayerPtr GetChild(const LayerId&);

        // Methods for the parent to access
        void SetUpCallbackTime();  // Used to pet the watchdog
        void RunCallbacks(const LayerId& lid, uint32_t falcon_status,
                          uint32_t remote_status);

        // RPC convenience function
        void InitRPCArgs(const LayerId& child, target_t* target,
                         client_addr_t* addr, const Generation* gen);

        // Information about this layer
        const LayerId                     handle_;
        const Generation                  gen_;
        bool                              killable_;
        CallbackList                      cb_list_;
        bool                              base_layer_;

        // Watchdog state
        Watchdog*                         watchdog_;

        // Layer structural information
        FalconParentPtr                   parent_;
        std::map<LayerId, FalconLayerPtr> children_;
        std::map<LayerId, int>            child_leaves_;

        // Synchronization
        pthread_mutex_t                   kill_lock_;
        pthread_mutex_t                   cancel_lock_;
        pthread_mutex_t                   add_child_lock_;

        pthread_cond_t                    kill_cond_;
        pthread_cond_t                    cancel_cond_;
        pthread_cond_t                    add_child_cond_;

        bool                              kill_active_;
        bool                              cancel_active_;
        bool                              add_child_active_;

        void                              LockAll();
        void                              UnlockAll();

        // RPC client for this layer
        CLIENT*                           clnt_;
        int                               fd_;

        // Client address with tag for this layer
        client_addr_t                     addr_;
};
#endif  // _NTFA_FALCON_CLIENT_FALCONLAYER_H_
