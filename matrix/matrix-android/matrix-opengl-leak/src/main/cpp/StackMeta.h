#ifndef MATRIX_ANDROID_STACKMETA_H
#define MATRIX_ANDROID_STACKMETA_H

#define STACK_SPLAY_MAP_CAPACITY 1024

#include <cstdio>
#include "Backtrace.h"
#include "splay_map.h"

struct __attribute__((__packed__)) stack_meta_t {
    size_t ref_count;
    wechat_backtrace::Backtrace* backtrace;
    void *ext;
};

static splay_map<uint64_t, stack_meta_t> backtrace_map = splay_map<uint64_t, stack_meta_t>(STACK_SPLAY_MAP_CAPACITY);

wechat_backtrace::Backtrace *deduplicate_backtrace(wechat_backtrace::Backtrace *backtrace);

bool delete_backtrace(wechat_backtrace::Backtrace *backtrace);

#endif //MATRIX_ANDROID_STACKMETA_H
