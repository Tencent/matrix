//
// Created by Yves on 2019/8/5.
//
// deprecated
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <pthread.h>
#include "lock.h"

volatile std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//volatile bool m_lock = false;

//void init_lock(unsigned char *buff) {
//
//}

//void acquire_lock_log() {
//    acquire_lock();
//}

void acquire_lock() {
    pthread_mutex_lock(&mutex);
//    while (m_lock.test_and_set(std::memory_order_acquire)) {
////        LOGD("Yves", "tid = %d is waiting", gettid());
//    }

//    while(!__sync_bool_compare_and_swap(&m_lock, false, true)) {
//
//    }

//    while (__atomic_exchange_n(&m_lock, true, __ATOMIC_ACQUIRE));
}

void release_lock() {
    pthread_mutex_unlock(&mutex);
//    assert(m_lock != 0);
//    m_lock.clear(std::memory_order_release);
//    __sync_bool_compare_and_swap(&m_lock, true, false);
//    while (!__atomic_exchange_n(&m_lock, false, __ATOMIC_RELEASE));
}
