//
// Created by tomystang on 2020/11/26.
//

#ifndef __MEMGUARD_MUTEX_H__
#define __MEMGUARD_MUTEX_H__


#include <cstddef>
#include <pthread.h>
#include <util/Auxiliary.h>

namespace memguard {
    class Mutex {
    public:
        Mutex(): mRawMutex(PTHREAD_MUTEX_INITIALIZER) {}

        bool tryLock();

        void lock();

        void unlock();

    private:
        DISALLOW_COPY(Mutex);
        DISALLOW_MOVE(Mutex);
        DISALLOW_NEW(Mutex);

        pthread_mutex_t mRawMutex;
    };
}


#endif //__MEMGUARD_MUTEX_H__
