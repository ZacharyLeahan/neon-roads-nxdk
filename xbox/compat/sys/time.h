#pragma once

#include <lwip/sockets.h>
#include <time.h>

static inline int gettimeofday(struct timeval *value, void *timezone)
{
    struct timespec now;
    (void)timezone;
    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return -1;
    }
    value->tv_sec = (long)now.tv_sec;
    value->tv_usec = now.tv_nsec / 1000;
    return 0;
}
