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
#ifndef _NTFA_FALCON_CLIENT_WATCHDOG_H_
#define _NTFA_FALCON_CLIENT_WATCHDOG_H_

#include <pthread.h>

#include "FalconLayer.h"

// This class holds the state necessary for cancellation and triggering the end
// to end timeout.
class Watchdog {
    public:
        Watchdog(FalconLayerPtr start_layer, LayerId top_name,
                 uint32_t timeout);
        // Pet the watchdog to notify the client of signs of life
        void Pet();
        // Stop should only be called once, it deletes the object
        void Stop();
        // Cancel is like stop, except it doesn't start a thread to wait
        // for the dog to go away.
        void Cancel();

        // Start and stop timer
        void StartTimer(FalconCallbackPtr cb, int timeout);
        void StopTimer();

        // Thread friends.
        friend void* start_watchdog(void*);
        friend void* stop_watchdog(void*);
    private:
        // Private destructor so Stop can be non-blocking
        ~Watchdog();
        void            Run();
        bool            IsStopping();

        // Watchdog stuff
        FalconParentPtr start_layer_;
        LayerId         start_target_;
        timespec        curr_timeout_;

        // Threading stuff
        pthread_t       tid_;
        pthread_mutex_t watchdog_lock_;
        pthread_cond_t  watchdog_cond_;
        bool            started_;
        bool            stopping_;
        
        // To start the killing
        bool            aggravated_;
        bool            has_timeout_;

        FalconCallbackPtr up_callback_;
        // No copies!
        Watchdog(const Watchdog&);
        Watchdog& operator=(const Watchdog&);
};

const int32_t kFalconSpyRetry = 3;
#endif  // _NTFA_FALCON_CLIENT_WATCHDOG_H_
