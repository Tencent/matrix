//
// Created by Yves on 2020/6/22.
//

#ifndef LIBWXPERF_JNI_MEMORYHOOKMETAS_H
#define LIBWXPERF_JNI_MEMORYHOOKMETAS_H

#include <map>
#include <vector>
#include <set>
#include "unwindstack/Unwinder.h"
// TODO #include "pool_allocator.h"

#define TAG "Container"

struct ptr_meta_t {
    void     *ptr;
    size_t   size;
    void     *caller;
    uint64_t stack_hash;
    bool     is_mmap;
//    bool     recycled;

//    ptr_meta_t &operator=(const ptr_meta_t& r) {
//
//    }
};

struct caller_meta_t {
    size_t                 total_size;
    std::set<const void *> pointers;
};

struct stack_meta_t {
    size_t size;
    void *caller;
    std::vector<unwindstack::FrameData> stacktrace;
};


// ==========================================
//template<class _Tp>
//struct base_selector {
//
//    virtual size_t capacity() = 0;
//
//    virtual size_t select(_Tp __v) = 0;
//};

//struct default_pointer_selector : public base_selector<uintptr_t> {
//    inline size_t operator()(unsigned char __v) const _NOEXCEPT {
//        return static_cast<size_t>(__v & MASK);
//    }
//
//    inline size_t capacity() override {
//        return MAX_SLOT;
//    }
//
//    inline size_t select(uintptr_t __v) override {
//        return static_cast<size_t>(__v & MASK);
//    }
//
//private:
//    static const unsigned int MAX_SLOT = 1 << 16;
//    static const unsigned int MASK     = MAX_SLOT - 1;
//};

// TODO
//template <class _Tp, class _Container>
//class base_container {
//
//};
// ==========================================
template <class _Key, class _Val>
class concurrent_slot_maps {

    typedef struct {

        std::map<_Key, _Val> container;
        std::mutex           mutex;

    } container_wrapper;

#define TARGET_WITH_LOCK(target, key) \
    container_wrapper * target = container_slots.data()[select((uintptr_t) __k)]; \
    std::lock_guard<std::mutex> target_lock(target->mutex)

public:
    concurrent_slot_maps() {
        const size_t cap = capacity();
        container_slots.reserve(cap);
        for (int i = 0; i < cap; ++i) {
            container_slots.push_back(new container_wrapper);
        }
    }

    inline _Val &operator[](_Key __k) {
        TARGET_WITH_LOCK(target, __k);
        return target->container[__k];
    }

    template<class _Callable>
    inline void insert(_Key __k, _Callable __callable) {
        TARGET_WITH_LOCK(target, __k);
        auto &meta = target->container[__k];
        if constexpr (!std::is_same<std::nullptr_t, decltype(__callable)>::value) {
            __callable(meta);
        }
    }

    inline _Val &at(_Key __k) {
        TARGET_WITH_LOCK(target, __k);
        auto &ret = target->container.at(__k);
        return target->container.at(__k);
    }

    template<class _Callable>
    inline void get(_Key __k, _Callable __callable) {
        TARGET_WITH_LOCK(target, __k);
        if (0 != target->container.count(__k)) {
            auto &meta = target->container.at(__k);
            if constexpr (!std::is_same<std::nullptr_t, decltype(__callable)>::value) {
                __callable(meta);
            }
        }
    }

    /**
     * fake erase, 保留 map 中的结点
     * @param __k
     * @param __out_meta
     * @return
     */
    template<class _Callback>
    inline bool erase(_Key __k, _Callback __callback) {
        TARGET_WITH_LOCK(target, __k);

        auto it = target->container.find(__k);
        if (it == target->container.end()) { // not contains
            return false;
        }
//        it->second.recycled = true; // mark recycled. TODO

        if constexpr (!std::is_same<std::nullptr_t, decltype(__callback)>::value) {
            __callback(it->second);
        }

        target->container.erase(it);

        return true;
    }

    bool contains(_Key __k) {
        TARGET_WITH_LOCK(target, __k);
        return 0 != target->container.count(__k);
    }

    template<class _Callback>
    void for_each(_Callback __func) {
        for (const auto cw : container_slots) {
            std::lock_guard<std::mutex> container_lock(cw->mutex);

            for (auto it : cw->container) {
                __func(it.first, it.second);
            }

            // TODO
//            for (auto it = cw->container.begin(); it != cw->container.end();) {
//                if (!it->second.recycled) {
//                    __func(it->first, it->second);
//                    it++;
//                } else {
//
//                    cw->container.erase(it++); // real erase
//                }
//            }
        }
    }

    size_t size() {
        size_t count = 0;
        for (const auto cw : container_slots) {
            std::lock_guard<std::mutex> container_lock(cw->mutex);
            count += cw->container.size();
        }
        return count;
    }

private:

    static inline size_t capacity() {
        return MAX_SLOT;
    }

    static inline size_t select(uintptr_t __v) {
        return static_cast<size_t>((__v ^ (__v >> 16)) & MASK);
    }

    std::vector<container_wrapper *> container_slots;
    static const unsigned int        MAX_SLOT = 1 << 10;
    static const unsigned int        MASK     = MAX_SLOT - 1;
};

#undef TAG

#endif //LIBWXPERF_JNI_MEMORYHOOKMETAS_H
