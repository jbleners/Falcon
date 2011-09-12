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
#ifndef _NTFA_FALCON_CLIENT_FALCONCALLBACK_H_
#define _NTFA_FALCON_CLIENT_FALCONCALLBACK_H_
#include <stdint.h>
#include <pthread.h>

#include <boost/smart_ptr.hpp>

#include <string>
#include <vector>

#include "common.h"
// For convenience
typedef std::string LayerId;
typedef std::vector<LayerId> LayerIdList;

// This is the raw function passed by the client
// handle        is the fully qualified handle of the application for this
//               callback
// client_data   pointer to opaque data for use by the callback
// falcon_status indicates whether this was caused by a remote or local event
//               if it is a remote event, it indicates the layer
// remote_status this is the status provided by the remote spy
typedef void(*falcon_callback_fn)(const LayerIdList& handle,
                                  void* client_data,
                                  uint32_t falcon_status,
                                  uint32_t remote_status);
struct cb_package;

// FalconCallback wraps the raw function provided by a client into a curried
// function with some synchronization goodies:
// The () operator will only call the function f once per object, so the
// client is guaranteed exactly one callback per registration
class FalconCallback {
    public:
        FalconCallback(falcon_callback_fn f, const LayerIdList& h, void* cd,
                       bool repeatable=true) :
            f_(f), h_(h), cd_(cd), repeatable_(repeatable), run_final_(false) {
            CHECK(0 == pthread_mutex_init(&cb_lock_, NULL));
        }

        bool Reactivate(falcon_callback_fn f, void* client_data=NULL);
        void Deactivate();

        bool HasRunFinal();

        ~FalconCallback();
        void operator() (const LayerId& lid, uint32_t falcon_status,
                         uint32_t remote_status);

        void SetData(void* new_data);

    private:
        falcon_callback_fn  f_;
        const LayerIdList   h_;
        void*               cd_;
        pthread_mutex_t     cb_lock_;
        bool                repeatable_;
        bool                run_final_; 

        uint32_t            deferred_falcon_status_;
        uint32_t            deferred_remote_status_;
        int                 deferred_index_;
        // No copying, etc, etc...
        FalconCallback& operator=(const FalconCallback&);
        FalconCallback(FalconCallback&);
};

typedef boost::shared_ptr<FalconCallback> FalconCallbackPtr;
#endif  // _NTFA_FALCON_CLIENT_FALCONCALLBACK_H_
