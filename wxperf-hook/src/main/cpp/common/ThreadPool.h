//
// Created by Yves on 2020/6/15.
//

#ifndef LIBWXPERF_JNI_THREADPOOL_H
#define LIBWXPERF_JNI_THREADPOOL_H


#include <cstddef>
#include <vector>
#include <thread>
#include <condition_variable>
#include <semaphore.h>
#include <queue>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <linux/eventpoll.h>
#include "Log.h"

#define TAG "ThreadPool"

typedef std::function<void()> runnable;

class worker {

#define EPOLL_MAX_EVENTS 16
#define DEFAULT_WAIT_TIMEOUT_MILLIS 1

public:
    worker() : worker("mh-worker-def") {
    }

    worker(const char *__name) : worker(__name, -1) {
    }

    worker(const char *__name, const int __epfd) : thread_name(__name),
                                                   epoll_fd(__epfd),
                                                   quit(false),
                                                   wait_timeout_millis(
                                                           DEFAULT_WAIT_TIMEOUT_MILLIS) {
//        epoll_prepare();
        this->work_thread = std::thread(worker::work, this);
    }

    ~worker() {
        {
            std::lock_guard<std::mutex> lock(task_mutex);
            quit = true;
        }
//        task_condition.notify_one();
        work_thread.join();
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, wake_fd, nullptr);
        close(epoll_fd);
    }

    void enqueue(runnable __task) {
//        LOGD(TAG, "worker execute");
//        NanoSeconds_Start(enq_begin);
        bool need_wake;
        {
            std::lock_guard<std::mutex> queue_lock(task_mutex);
            task_queue.emplace(__task);
            need_wake = blocked;
        }

//        NanoSeconds_End(enq_cost, enq_begin);
//
//        NanoSeconds_Start(notify_begin);
        if (need_wake) {
            task_condition.notify_one();
//            epoll_notify();
        }
//        NanoSeconds_End(notify_cost, notify_begin);
////        if (!need_wake)
//        LOGD(TAG, "acquire lock enq cost %lld, notify cost %s, %lld",
//             enq_cost, need_wake ? "true" : "false", notify_cost);
    }

private:

    static void work(worker *w) {
        pthread_setname_np(w->work_thread.native_handle(), w->thread_name.c_str());

//        int time_out_millis = 0;
        for (;;) {

            runnable task;
            size_t   retained_size;
            {
                std::unique_lock<std::mutex> queue_lock(w->task_mutex);

                w->task_condition.wait(queue_lock, [w] {
                    w->blocked = w->task_queue.empty();
                    if (w->blocked) {
                        LOGD(TAG, "task queue is empty, blocked");
                    }
                    return w->quit || !w->blocked;
                });

                if (w->quit) {
                    return;
                }

                w->wait_timeout_millis = DEFAULT_WAIT_TIMEOUT_MILLIS;
                w->blocked             = false;
                task = w->task_queue.front();
                w->task_queue.pop();
                retained_size = w->task_queue.size();
            }
//            LOGD(TAG, "proceed within #%s retained %zu", w->thread_name.c_str(), retained_size);
            task();
//            usleep(0);
        }
    }

    std::thread                        work_thread;
    std::queue<std::function<void()> > task_queue;
    std::condition_variable            task_condition;
    std::mutex                         task_mutex;
    std::string                        thread_name;
    int                                wake_fd;
    int                                epoll_fd;
    int                                wait_timeout_millis;
    bool                               quit;
    bool                               blocked;
//    bool                               hungry;

};

template<class _Tp>
struct default_selector {
    inline size_t operator()(_Tp __v, const size_t &__max) const _NOEXCEPT {
        return std::hash<_Tp>()(__v) % __max;
    }
};

class multi_worker_thread_pool {

public:

    multi_worker_thread_pool() : multi_worker_thread_pool(std::thread::hardware_concurrency()) {}

    multi_worker_thread_pool(unsigned int __log_size) : log_pool_size(__log_size),
                                                        pool_size(std::pow(2, __log_size)) {
        LOGD(TAG, "pool size = %zu, log_pool_size = %u", pool_size, log_pool_size);
        assert(pool_size > 0);
        assert(log_pool_size < 6);
        workers = static_cast<worker **>(malloc(sizeof(worker *) * pool_size));

        for (int i = 0; i < pool_size; ++i) {
            std::stringstream ss;
            ss << "mh-worker-" << i;
            workers[i] = new worker(ss.str().c_str());
        }
    }

    ~multi_worker_thread_pool() {
        for (int i = 0; i < pool_size; ++i) {
            delete workers[i];
            workers[i] = nullptr;
        }
    }

    size_t worker_choose(uint64_t __val) {

        auto     ret = __val;
        for (int i   = log_pool_size; i < 64; i += std::max(log_pool_size, 1u)) {
            ret ^= __val >> i;
        }

        ret = ret & (pool_size - 1);
        LOGD(TAG, "worker_choose %lu & %zu -> %zu", __val, (pool_size - 1), ret);
        return ret;
    }

    void execute(runnable __task) const {
        execute(__task, -1);
    }

    // fixme 减少拷贝
    void execute(runnable __func, size_t __idx) const {
        if (__idx < 0 || __idx >= pool_size) {
            __idx = pthread_self() & (pool_size - 1);
        }

        assert(__idx < pool_size);
//        NanoSeconds_Start(begin);
        workers[__idx]->enqueue(__func);
//        NanoSeconds_End(end, begin);
//        LOGD(TAG, "enqueue cost %lld", end);
    }

private:

    size_t       pool_size;
    unsigned int log_pool_size;
    worker       **workers;
};

#undef TAG
#endif //LIBWXPERF_JNI_THREADPOOL_H
