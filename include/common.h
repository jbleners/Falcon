#ifndef _NTFA_COMMON_COMMON_H_
#define _NTFA_COMMON_COMMON_H_
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include <string>

// A check macro
#define CHECK(x) do { \
    if(!(x)) { \
        perror("CHECK"); \
        fprintf(stderr, "%s:%s:%d:\n", __FILE__, __func__, __LINE__); \
        assert((x)); \
    } \
} while(0);
// a log macro
#ifndef LOG
#define LOG(fmt,args...) do { \
    fprintf(stderr, "%f:%s:%s:%d: " fmt "\n", GetRealTime(),__FILE__,__func__,__LINE__ ,##args); \
    fflush(stderr);\
} while (0);
#endif

#define LOG1(x) LOG("%s",x)

// Some time conversion constants
#define kMillisecondsToSeconds 1e-3
#define kSecondsToMilliseconds 1000L
#define kMillisecondsToNanoseconds (1000L * 1000L)
#define kSecondsToNanoseconds (kSecondsToMilliseconds * kMillisecondsToNanoseconds)
#define kNanosecondsToSeconds 1e-9
#define kNanosecondsToMilliseconds 1e-6
#define kMillisecondsToMicroseconds 1000
#define kMicrosecondsToNanoseconds 1000

// Returns the realtime in seconds
inline double
GetRealTime() {
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time.tv_sec + kNanosecondsToSeconds * time.tv_nsec;
}

inline void
IncrementTimespecMilliseconds(struct timespec* tp, long delay_ms) {
    double ns = tp->tv_nsec + (double)delay_ms * kMillisecondsToNanoseconds;
    tp->tv_sec += ns * kNanosecondsToSeconds;
    tp->tv_nsec = ((long)ns) % kSecondsToNanoseconds;
}

inline void
IncrementTimespecSeconds(struct timespec* tp, double seconds) {
    time_t round_seconds = static_cast<time_t>(ceil(seconds));
    long nanoseconds = static_cast<long>((seconds - ceil(seconds)) 
                                         * kSecondsToNanoseconds);
    tp->tv_sec += round_seconds + ((unsigned long) (nanoseconds + tp->tv_nsec))/kSecondsToNanoseconds;
    tp->tv_nsec = ((unsigned long)(nanoseconds + tp->tv_nsec))%kSecondsToNanoseconds;
}

inline double
TimespecDiff(const struct timespec* tp1, const struct timespec* tp2) {
    return (tp1->tv_sec + kNanosecondsToSeconds * tp1->tv_nsec) - (tp2->tv_sec + kNanosecondsToSeconds * tp2->tv_nsec);
}

inline bool
HostLookup(const std::string& hostname, sockaddr_in* addr) {
        struct addrinfo hints;
        struct addrinfo* ret;
        memset(&hints, '\0', sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (0 != getaddrinfo(hostname.c_str(), NULL, &hints, &ret)) return false;
        memcpy(addr, ret->ai_addr, ret->ai_addrlen);
        freeaddrinfo(ret);
        return true;
}

const uint16_t kFalconPort = 22222;
#endif // _NTFA_COMMON_COMMON_H_
