//
// Created by tomystang on 2020/11/27.
//

#ifndef __MEMGUARD_THREAD_H__
#define __MEMGUARD_THREAD_H__


#include <cstring>
#include <utility>
#include "Auxiliary.h"

namespace memguard {
    class Thread {
    public:
        enum : int {
            RESULT_SUCCESS = 0,
            RESULT_INITIALIZE_FAILED = -9001,
            RESULT_CLONE_FAILED = -9002,
            RESULT_WAIT_FAILED = -9003
        };

        typedef int (*RoutineFunction)(void*);

        struct FixStackForART {};
        static inline constexpr const struct FixStackForART FixStackForART = {};

        Thread(size_t stack_size, const struct FixStackForART&, void* ucontext);

        explicit Thread(size_t stack_size, void* stack_hint = nullptr, bool force_hint = false)
            : Thread(stack_size, std::make_pair(stack_hint, force_hint)) {}

        Thread(Thread&& other) : mStack(other.mStack), mStackSize(other.mStackSize) {
            other.mStack = nullptr;
            other.mStackSize = 0;
        }

        Thread& operator =(Thread&& rhs) {
            destroy();
            mStack = rhs.mStack;
            mStackSize = rhs.mStackSize;
            rhs.mStack = nullptr;
            rhs.mStackSize = 0;
            return *this;
        }

        virtual ~Thread() {
            destroy();
        }

        int startAndWait(RoutineFunction routine, void* param, int* return_value);

        void destroy();

    private:
        DISALLOW_COPY(Thread);

        Thread(size_t stack_size, const std::pair<void*, bool>& hint);

        void* mStack;
        size_t mStackSize;
    };
}


#endif //__MEMGUARD_THREAD_H__
