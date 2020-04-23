//
// Created by Yves on 2019-08-12.
//

#include <cassert>
#include "utils.h"
#include "log.h"

#define JAVA_LONG_MAX_VALUE 0x7fffffffffffffff

uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size) {
    assert(p_pc_stacks != NULL && stack_size > 0);
    uint64_t sum = 0;
    for (int i = 0; i < stack_size; ++i) {
        sum += p_pc_stacks[i];
    }
    return sum;
}

uint64_t hash_stack_frames(std::vector<unwindstack::FrameData> &stack_frames) {
    uint64_t sum = 0;
    for (auto i = stack_frames.begin(); i != stack_frames.end(); ++i) {
//        LOGD("DEBUG", "i->pc = %p", i->pc);
        sum += i->pc;
    }
    return sum;
}

uint64_t hash_str(const char * str) {
    if (!str) {
        LOGD("DEBUG", "str is NULL");
    }

    int seed = 31;
    uint64_t hash = 0;
    while (*str) {
        hash = hash * seed + (*str++);
    }
    return hash;
}

uint64_t hash_combine(uint64_t l, uint64_t r) {
    return (l + r) & JAVA_LONG_MAX_VALUE;
}