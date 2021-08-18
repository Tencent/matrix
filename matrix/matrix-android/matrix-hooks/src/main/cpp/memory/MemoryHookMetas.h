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
#include "common/tree/splay_map.h"

#define TAG "Matrix.MemoryHook.Container"

#define USE_MEMORY_MESSAGE_QUEUE true
#define USE_SPLAY_MAP_SAVE_STACK true

/* For testing */
#define USE_FAKE_BACKTRACE_DATA false

#define MEMHOOK_BACKTRACE_MAX_FRAMES MAX_FRAME_SHORT

struct ptr_meta_t {
    void     *ptr;
    size_t   size;
    void     *caller;
    uint64_t stack_hash;
    bool     is_mmap;
};

struct caller_meta_t {
    size_t                 total_size;
    std::set<const void *> pointers;
};

typedef wechat_backtrace::BacktraceFixed<MEMHOOK_BACKTRACE_MAX_FRAMES> MemoryHookBacktrace;

struct stack_meta_t {
    /**
     * size 在分配释放阶段仅起引用计数作用, 因为在 dump 时会重新以 ptr_meta 的 size 进行统计
     */
    size_t                              size;
    void                                *caller;
    MemoryHookBacktrace                 backtrace;
};

typedef splay_map<const void *, ptr_meta_t> memory_map_t;
typedef splay_map<uint64_t, stack_meta_t> stack_map_t;

static const unsigned int MAX_PTR_SLOT = 1 << 8;
static const unsigned int PTR_MASK = MAX_PTR_SLOT - 1;

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
        memory_map_t container = memory_map_t(10240);
        std::mutex                         mutex;
    } ptr_meta_container_wrapper_t;

    typedef struct {
#if USE_SPLAY_MAP_SAVE_STACK == true
        stack_map_t container  = stack_map_t(1024);
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

    static const unsigned int MAX_PTR_META_SLOT   = 1 << 0;
    static const unsigned int PTR_META_MASK       = MAX_PTR_META_SLOT - 1;
    static const unsigned int MAX_STACK_META_SLOT = 1 << 0;
    static const unsigned int STACK_META_MASK     = MAX_STACK_META_SLOT - 1;
};

#endif

#undef TAG

#endif //LIBMATRIX_HOOK_MEMORYHOOKMETAS_H
