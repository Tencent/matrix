//
// Created by Yves on 2020/6/22.
//

#ifndef LIBMATRIX_HOOK_MEMORYHOOKMETAS_H
#define LIBMATRIX_HOOK_MEMORYHOOKMETAS_H

#include <map>
#include <vector>
#include <set>
#include "unwindstack/Unwinder.h"
#include "Utils.h"
// TODO #include "pool_allocator.h"

#define TAG "Matrix.MemoryHook.Container"

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

struct stack_meta_t {
    /**
     * size 在分配释放阶段仅起引用计数作用, 因为在 dump 时会重新以 ptr_meta 的 size 进行统计
     */
    size_t                              size;
    void                                *caller;
    wechat_backtrace::Backtrace         backtrace;
};

class memory_meta_container {

    typedef struct {

        std::map<const void *, ptr_meta_t> container;
        std::mutex                         mutex;

    } ptr_meta_container_wrapper_t;

    typedef struct {
        std::map<uint64_t, stack_meta_t> container;
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

    inline ptr_meta_t &operator[](const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(target, __k);
        return target->container[__k];
    }

    inline void insert(const void *__ptr,
                       uint64_t __stack_hash,
                       std::function<void(ptr_meta_t *, stack_meta_t *)> __callback) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __ptr);
        auto ptr_meta = &ptr_meta_container->container[__ptr];
        ptr_meta->stack_hash = __stack_hash;
        if (__stack_hash) {
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, __stack_hash);
            auto stack_meta = &stack_meta_container->container[__stack_hash];
            __callback(ptr_meta, stack_meta);
        } else {
            __callback(ptr_meta, nullptr);
        }
    }

//    inline ptr_meta_t &at(const void *__k) {
//        TARGET_PTR_CONTAINER_LOCKED(target, __k);
//        auto &ret = target->container.at(__k);
//        return target->container.at(__k);
//    }

    template<class _Callable>
    inline void get(const void *__k, _Callable __callable) {
        TARGET_PTR_CONTAINER_LOCKED(target, __k);
        if (0 != target->container.count(__k)) {
            auto &meta = target->container.at(__k);
            __callable(meta);
        }
    }

//    template<class _Callback>
    inline bool erase(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);

        auto it = ptr_meta_container->container.find(__k);
        if (it == ptr_meta_container->container.end()) { // not contains
            return false;
        }

        auto &ptr_meta = it->second;

        if (ptr_meta.stack_hash) {
            TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta.stack_hash);
            if (stack_meta_container->container.count(ptr_meta.stack_hash)) {
                auto &stack_meta = stack_meta_container->container.at(ptr_meta.stack_hash);
                if (stack_meta.size > ptr_meta.size) { // 减去同堆栈的 size
                    stack_meta.size -= ptr_meta.size;
                } else { // 删除 size 为 0 的堆栈
                    stack_meta_container->container.erase(ptr_meta.stack_hash);
                }
            }
        }

        ptr_meta_container->container.erase(it);

        return true;
    }

    bool contains(const void *__k) {
        TARGET_PTR_CONTAINER_LOCKED(ptr_meta_container, __k);
        return 0 != ptr_meta_container->container.count(__k);
    }

//    template<class _Callback>
    void for_each(std::function<void(const void *, ptr_meta_t *, stack_meta_t *)> __callback) {
        for (const auto cw : ptr_meta_containers) {
            std::lock_guard<std::mutex> container_lock(cw->mutex);

            for (auto it : cw->container) {
                auto &ptr      = it.first;
                auto ptr_meta = &it.second;

                if (ptr_meta->stack_hash) {
                    TARGET_STACK_CONTAINER_LOCKED(stack_meta_container, ptr_meta->stack_hash);
                    stack_meta_t *stack_meta = nullptr;
                    if (stack_meta_container->container.count(ptr_meta->stack_hash)) {
                        stack_meta = &stack_meta_container->container.at(ptr_meta->stack_hash);
                    }
                    __callback(ptr, ptr_meta, stack_meta); // within lock scope
                } else {
                    __callback(ptr, ptr_meta, nullptr);
                }


            }
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

#undef TAG

#endif //LIBMATRIX_HOOK_MEMORYHOOKMETAS_H
