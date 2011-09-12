#include "Watchdog.h"
#include "FalconClient.h"

#include <sys/time.h>

void*
start_watchdog(void* arg) {
    Watchdog* the_dog = static_cast<Watchdog*>(arg);
    the_dog->Run();
    return NULL;
}

void*
stop_watchdog(void *arg) {
    Watchdog* the_dog = static_cast<Watchdog*>(arg);
    delete the_dog;
    pthread_detach(pthread_self());
    return NULL;
}

Watchdog::Watchdog(FalconLayerPtr start_layer, LayerId top_name,
                   uint32_t timeout) {
    start_layer_ = start_layer;
    start_target_ = top_name;
    started_ = false;
    stopping_ = false;
    aggravated_ = false;
    has_timeout_ = false;

    pthread_mutex_init(&watchdog_lock_, NULL);
    pthread_cond_init(&watchdog_cond_, NULL);

    pthread_mutex_lock(&watchdog_lock_);
    pthread_create(&tid_, NULL, start_watchdog, this);
    while (!started_) {
        pthread_cond_wait(&watchdog_cond_, &watchdog_lock_);
    }
    pthread_mutex_unlock(&watchdog_lock_);
}

Watchdog::~Watchdog() {
    pthread_mutex_lock(&watchdog_lock_);
    stopping_ = true;
    pthread_cond_signal(&watchdog_cond_);
    pthread_mutex_unlock(&watchdog_lock_);
    pthread_join(tid_, NULL);
}

void
Watchdog::Stop() {
    pthread_t p;
    pthread_create(&p, NULL, stop_watchdog, this);
    return;
}

void
Watchdog::Cancel() {
    pthread_mutex_lock(&watchdog_lock_);
    stopping_ = true;
    pthread_cond_signal(&watchdog_cond_);
    pthread_mutex_lock(&watchdog_lock_);
}

void
Watchdog::Pet() {
    pthread_mutex_lock(&watchdog_lock_);
    if (up_callback_) (*up_callback_)(start_target_, SIGN_OF_LIFE, 0);
    pthread_mutex_unlock(&watchdog_lock_);
}

// Helper function for time comparison
inline bool
operator>(const timespec& t1, const timespec& t2) {
    return (t1.tv_sec > t2.tv_sec) ||
           (t1.tv_sec == t2.tv_sec && t1.tv_nsec > t1.tv_nsec);
}

bool
Watchdog::IsStopping() {
    bool ret;
    pthread_mutex_lock(&watchdog_lock_);
    ret = stopping_;
    pthread_mutex_unlock(&watchdog_lock_);
    return ret;
}

void
Watchdog::StartTimer(FalconCallbackPtr cb, int timeout) {
    pthread_mutex_lock(&watchdog_lock_);
    up_callback_ = cb;
    has_timeout_ = true;
    clock_gettime(CLOCK_REALTIME, &curr_timeout_);
    curr_timeout_.tv_sec += timeout;
    pthread_cond_signal(&watchdog_cond_);
    pthread_mutex_unlock(&watchdog_lock_);
}

void
Watchdog::StopTimer() {
    pthread_mutex_lock(&watchdog_lock_);
    if (up_callback_) {
        // clear the callback.
    }
    has_timeout_ = false;
    pthread_cond_signal(&watchdog_cond_);
    pthread_mutex_unlock(&watchdog_lock_);
}

void
Watchdog::Run() {
    // This is where the magic happens.
    // Basically, the watchdog sits in a pthread_cond_timedwait loop until
    // its timeout expires, then it walks down the tree trying to kill (until
    // it's killed by the leaf node being deleted). When it gets to the bottom
    // of the tree (i.e. it timed out on all kill responses), the parent node
    // will put it into an infinite loop (per the spec)
    pthread_mutex_lock(&watchdog_lock_);
    started_ = true;
    pthread_cond_signal(&watchdog_cond_);
    timespec now;
    int retval;

    std::vector<FalconParentPtr> pp_list;
    std::vector<LayerId> id_list;
    FalconLayerPtr curr_layer = start_layer_.lock();
    LayerId curr_target = start_target_;
    while (curr_layer) {
        pp_list.push_back(curr_layer);
        id_list.push_back(curr_target);
        curr_target = curr_layer->GetLayerId();
        curr_layer = curr_layer->GetParent();
    }

    // Wait to get pet
    do {
        if (has_timeout_) {
            retval = pthread_cond_timedwait(&watchdog_cond_, &watchdog_lock_,
                                            &curr_timeout_);
        } else {
            retval = pthread_cond_wait(&watchdog_cond_, &watchdog_lock_);
        }
        // Make sure we acquired the lock.
        CHECK(retval == 0 || retval == ETIMEDOUT);
        clock_gettime(CLOCK_REALTIME, &now);
    } while ((!has_timeout_ || curr_timeout_ > now) &&
             !stopping_ &&
             !aggravated_);

    curr_layer = start_layer_.lock();
    curr_target = start_target_;
    pthread_mutex_unlock(&watchdog_lock_);

    // Start down the war path
    for (;;) {
        int ret;
        if (!curr_layer || IsStopping()) break;
        ret = curr_layer->KillChild(curr_target);
        LOG("Kill ret: curr_target: %s %d", curr_target.c_str(), ret);
        if (ret == 0 || IsStopping()) break;
        pthread_mutex_lock(&watchdog_lock_);
        clock_gettime(CLOCK_REALTIME, &now);
        timespec curr_timeout = now;
        // TODO(leners): Perhaps replace timeout_s_ with a different value?
        if (ret < 0) {
            curr_timeout.tv_sec += kFalconSpyRetry;
            do {
                retval = pthread_cond_timedwait(&watchdog_cond_,
                                                &watchdog_lock_, &curr_timeout);
                CHECK(retval == 0 || retval == ETIMEDOUT);
                clock_gettime(CLOCK_REALTIME, &now);
            } while (curr_timeout > now && !stopping_);
        }
        curr_target = curr_layer->GetLayerId();
        curr_layer = curr_layer->GetParent();
        pthread_mutex_unlock(&watchdog_lock_);
    }
    LOG("Watch dog beginning cancellation");
    // Start down the cancel path
    for (size_t i = 0; i < pp_list.size(); i++) {
        curr_layer = pp_list[i].lock();
        curr_target = id_list[i];
        if (curr_layer) {
            LOG("Attempting to cancel %s", curr_target.c_str());
            curr_layer->CancelChild(curr_target);
        } else {
            LOG("Didn't cancel %s", curr_target.c_str());
        }
    }
    return;
}
