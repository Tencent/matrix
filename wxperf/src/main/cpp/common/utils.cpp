//
// Created by Yves on 2019-08-12.
//

#include <cassert>
#include "utils.h"

uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size) {
    assert(p_pc_stacks != NULL && stack_size > 0);
    uint64_t sum = 0;
    for (int i = 0; i < stack_size; ++i) {
        sum += p_pc_stacks[i];
    }
    return sum;
}

uint64_t hash(std::vector<unwindstack::FrameData> &stack_frames) {
    assert(!stack_frames.empty());
    uint64_t sum = 0;
    for (auto i = stack_frames.begin(); i != stack_frames.end(); ++i) {
        sum += i->pc;
    }
    return sum;
}
