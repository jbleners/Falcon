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
