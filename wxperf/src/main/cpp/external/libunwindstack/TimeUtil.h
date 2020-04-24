//
// Created by Yves on 2020-04-14.
//

#ifndef LIBWXPERF_JNI_TIMEUTIL_H
#define LIBWXPERF_JNI_TIMEUTIL_H

inline long CurrentNano() {
    struct timespec tms;

    if (clock_gettime(CLOCK_REALTIME,&tms)) {
//        LOGE("Unwind-debug", "get time error");
    }

    return tms.tv_nsec;
}

#endif //LIBWXPERF_JNI_TIMEUTIL_H
