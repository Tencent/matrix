//
// Created by tomystang on 2020/11/26.
//

#include <util/Log.h>
#include <util/Mutex.h>
#include <cerrno>
#include <cstring>

using namespace memguard;

#define LOG_TAG "MemGuard.Mutex"

bool memguard::Mutex::tryLock() {
    int ret = pthread_mutex_trylock(&mRawMutex);
    if (LIKELY(ret == 0)) {
        return true;
    } else {
        if (ret != EBUSY) {
            LOGE(LOG_TAG, "Fail to hold mutex without blocking, error: %s(%d)", strerror(ret), ret);
        }
        return false;
    }
}

void memguard::Mutex::lock() {
    int ret = pthread_mutex_lock(&mRawMutex);
    if (ret != 0) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to hold mutex, error: %s(%d)", strerror(errcode), errcode);
    }
}

void memguard::Mutex::unlock() {
    int ret = pthread_mutex_unlock(&mRawMutex);
    if (ret != 0) {
        int errcode = errno;
        LOGE(LOG_TAG, "Fail to hold mutex, error: %s(%d)", strerror(errcode), errcode);
    }
}