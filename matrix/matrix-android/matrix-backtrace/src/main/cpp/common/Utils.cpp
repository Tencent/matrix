//
// Created by Yves on 2019-08-12.
//

#include <cassert>
#include "Utils.h"
#include "Log.h"
#include "Backtrace.h"

#define JAVA_LONG_MAX_VALUE 0x7fffffffffffffff

BACKTRACE_EXPORT uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size) {
    assert(p_pc_stacks != NULL && stack_size > 0);
    uint64_t sum = 0;
    for (size_t i = 0; i < stack_size; ++i) {
        sum += p_pc_stacks[i];
    }
    return sum;
}

BACKTRACE_EXPORT uint64_t hash_backtrace_frames(wechat_backtrace::Backtrace *backtrace) {
    uptr sum = 1;
    if (backtrace == nullptr) {
        return (uint64_t) sum;
    }
    for (size_t i = 0; i != backtrace->frame_size; i++) {
        sum += (backtrace->frames.get())[i].pc;
    }
    return (uint64_t) sum;
}

BACKTRACE_EXPORT uint64_t hash_str(const char * str) {
    if (!str) {
        LOGD("DEBUG", "str is NULL");
        return 0;
    }

    int seed = 31;
    uint64_t hash = 0;
    while (*str) {
        hash = hash * seed + (*str++);
    }
    return hash;
}

BACKTRACE_EXPORT uint64_t hash_combine(uint64_t l, uint64_t r) {
    return (l + r) & JAVA_LONG_MAX_VALUE;
}