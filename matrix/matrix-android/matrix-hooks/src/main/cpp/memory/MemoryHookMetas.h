/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Created by Yves on 2020/6/22.
//

#ifndef LIBMATRIX_HOOK_MEMORYHOOKMETAS_H
#define LIBMATRIX_HOOK_MEMORYHOOKMETAS_H

#include <map>
#include <vector>
#include <set>
#include <common/Macros.h>
#include "unwindstack/Unwinder.h"
#include "Utils.h"
#include "common/struct/splay_map.h"
#include "MemoryBufferQueue.h"

#define TAG "Matrix.MemoryHook.Container"

struct __attribute__((__packed__)) ptr_meta_t {
    void *ptr;
    size_t size;
    uint64_t stack_hash;
#if USE_STACK_HASH_NO_COLLISION == true
    union {
        uintptr_t caller;
        uintptr_t stack_idx;
        uintptr_t ext_stack_ptr;
    };
    struct {
        unsigned char is_stack_idx : 1;
        unsigned char is_mmap : 1;
    } attr;
#else
    uintptr_t caller;
    struct {
        unsigned char is_mmap : 1;
    } attr;
#endif
};

struct caller_meta_t {
    size_t total_size;
    std::set<const void *> pointers;
};

struct __attribute__((__packed__)) stack_meta_t {
    /**
     * size 在分配释放阶段仅起引用计数作用, 因为在 dump 时会重新以 ptr_meta 的 size 进行统计
     */
    size_t size;
    uintptr_t caller;
    matrix::memory_backtrace_t backtrace;
#if USE_STACK_HASH_NO_COLLISION == true
    void *ext;
#endif
};

typedef splay_map<const void *, ptr_meta_t> memory_map_t;
typedef splay_map<uint64_t, stack_meta_t> stack_map_t;

#if USE_MEMORY_MESSAGE_QUEUE == false

class memory_meta_container {

    typedef struct {
        memory_map_t container = memory_map_t(64);
        std::mutex                         mutex;
    } ptr_meta_container_wrapper_t;

    typedef struct {
#if USE_SPLAY_MAP_SAVE_STACK == true
        stack_map_t container  = stack_map_t(8);
#else
        std::map<uint64_t, stack_meta_t> container;
#endif
        std::mutex                       mutex;
    } stack_container_wrapper_t;

#define TARGET_PTR_CONTAINER_LOCKED(target, key) \
    ptr_meta_container_wrapper_t * target = ptr_meta_containers.data()[ptr_meta_hash((uintptr_t) key)]; \
    std::lock_guard<std::mutex> target_lock(target->mutex)

#define TARGET_STACK_CONTAINER_LOCKED(target, key) \
    stack_container_wrapper_t *target = stack_meta_containers.data()[stack_meta_hash(key)]; \
    std::lock_guard<std::mutex> stack_lock(target->mutex)

public:

    memory_meta_container() {
        size_t cap = ptr_meta_capacity();
        ptr_meta_containers.reserve(cap);
        for (int i = 0; i < cap; ++i) {
            ptr_meta_containers.emplace_back(new ptr_meta_container_wrapper_t);
        }

        cap = stack_meta_capacity();
        stack_meta_containers.reserve(cap);
        for (int i = 0; i < cap; ++i) {
            stack_meta_containers.emplace_back(new stack_container_wrapper_t);
        }
    }

    inline void insert(const void *__ptr,
                       uint64_t __stack_hash,
                       std::function<void(ptr_meta_t *, stack_meta_t *)> __callback) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __ptr);
        auto ptr_meta = ptr_meta_container->container.insert(__ptr, { 0 });

        if (UNLIKELY(ptr_meta == nullptr)) {
            return;
        }

        ptr_meta->stack_hash = __stack_hash;

        if (__stack_hash) {
            stack_meta_t *stack_meta;
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, __stack_hash);
#if USE_SPLAY_MAP_SAVE_STACK == true
            if (LIKELY(stack_meta_container->container.exist(__stack_hash))) {
                stack_meta = &stack_meta_container->container.find();
            } else {
                stack_meta = stack_meta_container->container.insert(__stack_hash, {0});
            }
#else
            auto it = stack_meta_container->container.find(__stack_hash);
            if (LIKELY(it != stack_meta_container->container.end())) {
                stack_meta = &it->second;
            } else {
                stack_meta = &stack_meta_container->container[__stack_hash];
            }
#endif
            __callback(ptr_meta, stack_meta);
        } else {
            __callback(ptr_meta, nullptr);
        }
    }

    template<class _Callable>
    inline void get(const void *__k, _Callable __callable) {
        TARGET_PTR_CONTAINER_LOCKED(target, __k);
        if (target->container.exist(__k)) {
            auto &meta = target->container.find();
            __callable(meta);
        }
    }

    inline bool erase(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);

        auto removed_ptr_meta = ptr_meta_container->container.remove(__k);

        if (UNLIKELY(!removed_ptr_meta)) { // not contains
            return false;
        }

        auto ptr_meta = *removed_ptr_meta;

        if (LIKELY(ptr_meta.stack_hash)) {
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta.stack_hash);
#if USE_SPLAY_MAP_SAVE_STACK == true
            if (LIKELY(stack_meta_container->container.exist(ptr_meta.stack_hash))) {
                auto &stack_meta = stack_meta_container->container.find();
                if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    stack_meta_container->container.remove(ptr_meta.stack_hash);
                }
            }
#else
            auto it = stack_meta_container->container.find(ptr_meta.stack_hash);
            if (LIKELY(it != stack_meta_container->container.end())) {
                auto &stack_meta = it->second;
                if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    stack_meta_container->container.erase(it);
                }
            }
#endif
        }

        return true;
    }

    bool contains(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);
        return ptr_meta_container->container.exist(__k);
    }

    void for_each(std::function<void(const void *, ptr_meta_t *, stack_meta_t *)> __callback) {
        for (const auto cw : ptr_meta_containers) {
            std::lock_guard<std::mutex> container_lock(cw->mutex);
            cw->container.enumerate([&](const void * ptr, ptr_meta_t &ptr_meta) {
                if (ptr_meta.stack_hash) {
                    TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta.stack_hash);
                    stack_meta_t *stack_meta = nullptr;
#if USE_SPLAY_MAP_SAVE_STACK == true
                    if (stack_meta_container->container.exist(ptr_meta.stack_hash)) {
                        stack_meta = &stack_meta_container->container.find();
                    }
#else
                    auto it = stack_meta_container->container.find(ptr_meta.stack_hash);
                    if (it != stack_meta_container->container.end()) {
                        stack_meta = &it->second;
                    }
#endif
                    __callback(ptr, &ptr_meta, stack_meta); // within lock scope
                } else {
                    __callback(ptr, &ptr_meta, nullptr);
                }
            });
        }
    }

private:

    static inline size_t ptr_meta_capacity() {
        return MAX_PTR_META_SLOT;
    }

    static inline size_t ptr_meta_hash(uintptr_t __key) {
        return static_cast<size_t>((__key ^ (__key >> 16)) & PTR_META_MASK);
    }

    static inline size_t stack_meta_capacity() {
        return MAX_STACK_META_SLOT;
    }

    static inline size_t stack_meta_hash(uint64_t __key) {
        return ((__key ^ (__key >> 16)) & STACK_META_MASK);
    }

    std::vector<ptr_meta_container_wrapper_t *> ptr_meta_containers;
    std::vector<stack_container_wrapper_t *>    stack_meta_containers;

    static const unsigned int MAX_PTR_META_SLOT   = 1 << 10;
    static const unsigned int PTR_META_MASK       = MAX_PTR_META_SLOT - 1;
    static const unsigned int MAX_STACK_META_SLOT = 1 << 9;
    static const unsigned int STACK_META_MASK     = MAX_STACK_META_SLOT - 1;
};

#else

class memory_meta_container {

    typedef struct {
        memory_map_t container = memory_map_t(PTR_SPLAY_MAP_CAPACITY);
        std::mutex mutex;
    } ptr_meta_container_wrapper_t;

    typedef struct {
#if USE_SPLAY_MAP_SAVE_STACK == true
        stack_map_t container = stack_map_t(STACK_SPLAY_MAP_CAPACITY);
#else
        std::map<uint64_t, stack_meta_t> container;
#endif
        std::mutex mutex;
    } stack_container_wrapper_t;

#define TARGET_PTR_CONTAINER_LOCKED(target, key) \
    ptr_meta_container_wrapper_t * target = ptr_meta_containers.data()[ptr_meta_hash((uintptr_t) key)]; \
    std::lock_guard<std::mutex> target_lock(target->mutex)

#define TARGET_STACK_CONTAINER_LOCKED(target, key) \
    stack_container_wrapper_t *target = stack_meta_containers.data()[stack_meta_hash(key)]; \
    std::lock_guard<std::mutex> stack_lock(target->mutex)

public:

    memory_meta_container() {
        size_t cap = ptr_meta_capacity();
        ptr_meta_containers.reserve(cap);
        for (int i = 0; i < cap; ++i) {
            ptr_meta_containers.emplace_back(new ptr_meta_container_wrapper_t);
        }

        cap = stack_meta_capacity();
        stack_meta_containers.reserve(cap);
        for (int i = 0; i < cap; ++i) {
            stack_meta_containers.emplace_back(new stack_container_wrapper_t);
        }
    }

    inline bool
    stacktrace_compare_common(matrix::memory_backtrace_t &a, matrix::memory_backtrace_t &b) {
        if (a.frame_size != b.frame_size) {
            return false;
        }

        bool same = true;
        for (size_t i = 0; i < a.frame_size; i++) {
            if (a.frames[i].pc == b.frames[i].pc) {
                continue;
            } else {
                same = false;
                break;
            }
        }

        return same;
    }

    inline void insert(
            const void *__ptr,
            uint64_t __stack_hash,
#if USE_SPLAY_MAP_SAVE_STACK == true
            matrix::allocation_message_t *allocation_message,
#endif
            std::function<void(ptr_meta_t *, stack_meta_t *)> __callback) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __ptr);

        auto ptr_meta = ptr_meta_container->container.insert(__ptr, {0});

        if (UNLIKELY(ptr_meta == nullptr)) {    // Commonly no memory
            return;
        }

        ptr_meta->stack_hash = __stack_hash;

        if (__stack_hash) {
            stack_meta_t *stack_meta;
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, __stack_hash);
#if USE_SPLAY_MAP_SAVE_STACK == true
            if (LIKELY(stack_meta_container->container.exist(__stack_hash))) {
                stack_meta = &stack_meta_container->container.find();
    #if USE_STACK_HASH_NO_COLLISION == true
                uint32_t last_ptr = stack_meta_container->container.root_ptr();
                bool same = false;

                CRITICAL_CHECK(stack_meta);
                CRITICAL_CHECK(last_ptr);

                stack_meta_t *target = stack_meta;
                stack_meta_t *target_ext = stack_meta;
                unsigned char is_top = 1;
                while (target_ext) {
                    if (allocation_message->caller == target_ext->caller) {
                        same = stacktrace_compare_common(allocation_message->backtrace,
                                                         target_ext->backtrace);
                    }
                    target = target_ext;
                    if (same) break;
                    is_top = 0;
                    target_ext = static_cast<stack_meta_t *>(target->ext);
                }

                if (UNLIKELY(!same)) {
                    if (stack_meta->size == 0 && stack_meta->caller == 0 &&
                        stack_meta->backtrace.frame_size == 0) {
                        target = stack_meta;    // Reuse first stack_meta.
                        is_top = 1;
                    } else {
                        target->ext = malloc(sizeof(stack_meta_t));
                        *static_cast<stack_meta_t *>(target->ext) = {0};
                        target = static_cast<stack_meta_t *>(target->ext);

                        is_top = 0;

                        // Statistic
                        matrix::BufferQueue::g_queue_extra_stack_meta_allocated.fetch_add(1, std::memory_order_relaxed);
                        matrix::BufferQueue::g_queue_extra_stack_meta_kept.fetch_add(1, std::memory_order_relaxed);
        #endif
                    }
                }

                ptr_meta->attr.is_stack_idx = is_top;
                if (is_top) {
                    ptr_meta->stack_idx = last_ptr;
                } else {
                    ptr_meta->ext_stack_ptr = reinterpret_cast<uint64_t>(target);
                }
                stack_meta = target;

    #endif
            } else {
                stack_meta = stack_meta_container->container.insert(__stack_hash, {0});
    #if USE_STACK_HASH_NO_COLLISION == true
                if (stack_meta) {
                    ptr_meta->stack_idx = stack_meta_container->container.root_ptr();
                    ptr_meta->attr.is_stack_idx = 1;
                }
    #endif
            }

            CRITICAL_CHECK(stack_meta);
            __callback(ptr_meta, stack_meta);
        } else {
            __callback(ptr_meta, nullptr);
        }
    }

    template<class _Callable>
    inline void get(const void *__k, _Callable __callable) {
        TARGET_PTR_CONTAINER_LOCKED(target, __k);
        if (target->container.exist(__k)) {
            auto &meta = target->container.find();
            __callable(meta);
        }
    }

    inline bool erase(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);

        auto removed_ptr_meta = ptr_meta_container->container.remove(__k);

        if (UNLIKELY(!removed_ptr_meta)) { // not contains
            return false;
        }

        auto ptr_meta = *removed_ptr_meta;

        if (LIKELY(ptr_meta.stack_hash)) {
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta.stack_hash);
#if USE_SPLAY_MAP_SAVE_STACK == true
            if (LIKELY(stack_meta_container->container.exist(ptr_meta.stack_hash))) {
    #if USE_STACK_HASH_NO_COLLISION == true
                auto &top_stack_meta = stack_meta_container->container.find();
                auto top_stack_idx = stack_meta_container->container.root_ptr();
                stack_meta_t *stack_meta;
                if (ptr_meta.attr.is_stack_idx) {

                    CRITICAL_CHECK(ptr_meta.stack_idx == top_stack_idx);

                    stack_meta = &stack_meta_container->container.get(ptr_meta.stack_idx);
                } else {
                    stack_meta = reinterpret_cast<stack_meta_t *>(ptr_meta.ext_stack_ptr);
                }
                if (stack_meta->size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta->size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    if (ptr_meta.attr.is_stack_idx) {

                        if (!top_stack_meta.ext) {
                            stack_meta_container->container.remove(ptr_meta.stack_hash);
                        } else {
                            top_stack_meta.size = 0;
                            top_stack_meta.caller = 0;
                            top_stack_meta.backtrace = {0};
                        }
                    } else {
                        stack_meta_t *prev = &top_stack_meta;
                        stack_meta_t *ext = static_cast<stack_meta_t *>(prev->ext);

                        CRITICAL_CHECK(prev);
                        CRITICAL_CHECK(prev->ext);

                        while (ext) {
                            if (ext == stack_meta) {
                                break;
                            }
                            prev = ext;
                            ext = static_cast<stack_meta_t *>(prev->ext);
                        }

                        CRITICAL_CHECK(ext);

                        if (ext) {
                            prev->ext = ext->ext;
                            free(ext);

                            // Statistic
                            matrix::BufferQueue::g_queue_extra_stack_meta_kept.fetch_sub(1, std::memory_order_relaxed);
                        }
                    }
                }
    #else
                auto &stack_meta = stack_meta_container->container.find();
                if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    stack_meta_container->container.remove(ptr_meta.stack_hash);
                }
    #endif
            }
#else
            auto it = stack_meta_container->container.find(ptr_meta.stack_hash);
            if (LIKELY(it != stack_meta_container->container.end())) {
                auto &stack_meta = it->second;
                if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    stack_meta_container->container.erase(it);
                }
            }
#endif
        }

        return true;
    }

    bool contains(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);
        return ptr_meta_container->container.exist(__k);
    }

    void for_each(std::function<void(const void *, ptr_meta_t *, stack_meta_t *)> __callback) {
        for (const auto cw : ptr_meta_containers) {
            std::lock_guard<std::mutex> container_lock(cw->mutex);
            cw->container.enumerate([&](const void *ptr, ptr_meta_t &ptr_meta) {
                if (ptr_meta.stack_hash) {
                    TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta.stack_hash);
                    stack_meta_t *stack_meta = nullptr;
#if USE_SPLAY_MAP_SAVE_STACK == true
                    if (LIKELY(stack_meta_container->container.exist(ptr_meta.stack_hash))) {
    #if USE_STACK_HASH_NO_COLLISION == true
                        if (ptr_meta.attr.is_stack_idx) {
                            stack_meta = &stack_meta_container->container.get(ptr_meta.stack_idx);
                        } else {
                            stack_meta = reinterpret_cast<stack_meta_t *>(ptr_meta.ext_stack_ptr);
                        }
    #else
                        stack_meta = &stack_meta_container->container.find();
    #endif
                    }
#else
                    auto it = stack_meta_container->container.find(ptr_meta.stack_hash);
                    if (it != stack_meta_container->container.end()) {
                        stack_meta = &it->second;
                    }
#endif
                    __callback(ptr, &ptr_meta, stack_meta); // within lock scope
                } else {
                    __callback(ptr, &ptr_meta, nullptr);
                }
            });
        }
    }

private:

    static inline size_t ptr_meta_capacity() {
        return MAX_PTR_META_SLOT;
    }

    static inline size_t ptr_meta_hash(uintptr_t __key) {
        return static_cast<size_t>((__key ^ (__key >> 16)) & PTR_META_MASK);
    }

    static inline size_t stack_meta_capacity() {
        return MAX_STACK_META_SLOT;
    }

    static inline size_t stack_meta_hash(uint64_t __key) {
        return ((__key ^ (__key >> 16)) & STACK_META_MASK);
    }

    std::vector<ptr_meta_container_wrapper_t *> ptr_meta_containers;
    std::vector<stack_container_wrapper_t *> stack_meta_containers;

#if USE_MEMORY_MESSAGE_QUEUE == true
    static const unsigned int MAX_PTR_META_SLOT = 1 << 0;
    static const unsigned int MAX_STACK_META_SLOT = 1 << 0;
#else
    static const unsigned int MAX_PTR_META_SLOT = 1 << 10;
    static const unsigned int MAX_STACK_META_SLOT = 1 << 9;
#endif

    static const unsigned int PTR_META_MASK = MAX_PTR_META_SLOT - 1;
    static const unsigned int STACK_META_MASK = MAX_STACK_META_SLOT - 1;
};

#endif

#undef TAG

#endif //LIBMATRIX_HOOK_MEMORYHOOKMETAS_H
