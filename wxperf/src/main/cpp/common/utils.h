//
// Created by Yves on 2019-08-12.
//

#ifndef LIBWXPERF_JNI_UTILS_H
#define LIBWXPERF_JNI_UTILS_H

#include <cstdint>
#include <unwindstack/Unwinder.h>

uint64_t hash_uint64(uint64_t *p_pc_stacks, size_t stack_size);
uint64_t hash_stack_frames(std::vector<unwindstack::FrameData> &stack_frames);
uint64_t hash_str(const char * str);
uint64_t hash_combine(uint64_t l, uint64_t r);

#endif //LIBWXPERF_JNI_UTILS_H
