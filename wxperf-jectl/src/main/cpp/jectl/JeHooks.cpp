//
// Created by Yves on 2020/8/20.
//

#include <cstddef>
#include "JeHooks.h"
#include "JeLog.h"

#define TAG "Wxperf.JeCtl.Hooks"

extent_hooks_t *origin_extent_hooks;
//bool arena_1 = false;

void *hook_extent_alloc(extent_hooks_t *extent_hooks, void *new_addr, size_t size,
                        size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
    if (arena_ind != 0) {
//        arena_1 = true;
    }
    LOGD(TAG, "hook_extent_alloc: size = %zu, arena_ind = %u", size, arena_ind); // should not log here
    return origin_extent_hooks->alloc(origin_extent_hooks, new_addr, size, alignment, zero, commit, arena_ind);
}

bool hook_extent_dalloc(extent_hooks_t *extent_hooks, void *addr, size_t size, bool committed,
                        unsigned arena_ind) {
    return origin_extent_hooks->dalloc(origin_extent_hooks, addr, size, committed, arena_ind);
}


void hook_extent_destroy(extent_hooks_t *extent_hooks, void *addr, size_t size, bool committed,
                         unsigned arena_ind) {
    origin_extent_hooks->destroy(origin_extent_hooks, addr, size, committed, arena_ind);
}

bool hook_extent_commit(extent_hooks_t *extent_hooks, void *addr, size_t size, size_t offset,
                        size_t length, unsigned arena_ind) {
    return origin_extent_hooks->commit(origin_extent_hooks, addr, size, offset, length, arena_ind);
}


bool hook_extent_decommit(extent_hooks_t *extent_hooks, void *addr, size_t size, size_t offset,
                          size_t length, unsigned arena_ind) {
    return origin_extent_hooks->decommit(origin_extent_hooks, addr, size, offset, length, arena_ind);
}

bool hook_extent_purge_lazy(extent_hooks_t *extent_hooks, void *addr, size_t size, size_t offset,
                            size_t length, unsigned arena_ind) {
    if (origin_extent_hooks->purge_lazy) {
        return origin_extent_hooks->purge_lazy(origin_extent_hooks, addr, size, offset, length,
                                               arena_ind);
    }
    return true; // 当 default_extent_hooks->purge_lazy == NULL, 返回 true 以保持 jemalloc 的逻辑一致
}

bool hook_extent_purge_forced(extent_hooks_t *extent_hooks, void *addr, size_t size, size_t offset,
                            size_t length, unsigned arena_ind) {
    if (origin_extent_hooks->purge_forced) {
        return origin_extent_hooks->purge_forced(origin_extent_hooks, addr, size, offset, length,
                                                 arena_ind);
    }
    return true; // 当 default_extent_hooks->purge_forced == NULL, 返回 true 以保持 jemalloc 的逻辑一致
}

extent_split_t *original_split;

bool hook_extent_split(extent_hooks_t *extent_hooks, void *addr, size_t size, size_t size_a,
                       size_t size_b, bool committed, unsigned arena_ind) {
//    bool ret = original_split(origin_extent_hooks, addr, size, size_a, size_b, committed, arena_ind);
//    LOGD(TAG, "hook_extent_split: arena_ind = %u, ret = %d", arena_ind, /*ret*/0);
//    return ret;
    return origin_extent_hooks->split(origin_extent_hooks, addr, size, size_a, size_b, committed, arena_ind);
}


bool hook_extent_merge(extent_hooks_t *extent_hooks, void *addr_a, size_t size_a, void *addr_b,
                       size_t size_b, bool committed, unsigned arena_ind) {
    return origin_extent_hooks->merge(origin_extent_hooks, addr_a, size_a, addr_b, size_b, committed,
                                      arena_ind);
}

extent_hooks_t extent_hooks = {
        hook_extent_alloc,
        hook_extent_dalloc,
        hook_extent_destroy,
        hook_extent_commit,
        hook_extent_decommit,
        hook_extent_purge_lazy,
        hook_extent_purge_forced,
        hook_extent_split,
        hook_extent_merge
};