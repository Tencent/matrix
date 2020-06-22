//
// Created by Yves on 2019-08-08.
// TODO 代码整理, 逻辑拆分
// TODO 大批量分配后大批量删除 case 的性能
// notice: 加锁顺序, 谨防死锁
//

#include <malloc.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <list>
#include <map>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include "MemoryHookFunctions.h"
#include "StackTrace.h"
#include "Utils.h"
#include "unwindstack/Unwinder.h"
#include "ThreadPool.h"
#include "MemoryHook.h"

struct ptr_meta_t {
    void      *ptr;
    size_t    size;
    void      *caller;
    uint64_t  stack_hash;
    bool      is_mmap;
    uint64_t  mem_stamp;
    pthread_t owner_thread;

    bool operator<(const ptr_meta_t &r) const {
        return mem_stamp < r.mem_stamp;
    }
};

struct caller_meta_t {
    size_t           total_size;
    std::set<void *> pointers;
};

struct stack_meta_t {
    size_t                              size;
    std::vector<unwindstack::FrameData> *p_stacktraces; // fixme using swap?
};

// fixme remove ?
struct ptr_meta_hash {
    // TODO mem_stamp 可作为唯一标识？
    std::size_t operator()(const ptr_meta_t &meta) const {
        return ((std::hash<void *>()(meta.ptr) * 31
                 + std::hash<void *>()(meta.caller)) * 31
                + std::hash<uint64_t>()(meta.stack_hash)) * 31
               + std::hash<uint64_t>()(meta.mem_stamp) + meta.is_mmap;
    }
};

struct ptr_meta_eq {
    bool operator()(const ptr_meta_t &l, const ptr_meta_t &r) const {
        return l.ptr == r.ptr && l.size == r.size && l.caller == r.caller &&
               r.stack_hash == r.stack_hash && l.is_mmap == r.is_mmap && l.mem_stamp == r.mem_stamp;
    }
};

struct tsd_t {

    std::mutex                        tsd_mutex;
    pthread_t                         owner_thread;
    std::multimap<void *, ptr_meta_t> ptr_metas;
    std::map<uint64_t, stack_meta_t>  stack_metas;
    std::multimap<void *, uint64_t>   borrowed_ptrs;
    size_t                            worker_idx;

    tsd_t() {
        owner_thread = pthread_self();
    }
};

// fixme use template ?
struct PtrHash {
    size_t operator()(void *ptr) const {
        auto uptr = (uintptr_t) ptr;

#ifndef __LP64__
        auto ret =  uptr ^ (uptr << 8) ^ (uptr << 16) ^ (uptr << 24);
#else
        auto ret = uptr ^(uptr << 16) ^(uptr << 32) ^(uptr << 48) ^(uptr << 56);
#endif
        return ret;
    }
};

struct PtrEqual {
    bool operator()(const void *__x, const void *__y) const {
        return __x == __y;
    }
};

struct PthreadHash {
    size_t operator()(const pthread_t &pthread) const {

        return pthread ^ (pthread << 8) ^ (pthread << 16) ^ (pthread << 24);
    }
};

struct PthreadEqual {
    bool operator()(const pthread_t &__x, const pthread_t &__y) const {
        return __x == __y;
    }
};

struct merge_bucket_t {
    std::shared_mutex                      bucket_shared_mutex;
    std::map<void *, std::set<ptr_meta_t>> ptr_metas;
    std::map<uint64_t, stack_meta_t>       stack_metas;
    std::map<void *, std::set<uint64_t>>   borrowed_ptrs;

    std::unordered_map<pthread_t, std::unordered_set<void *, PtrHash, PtrEqual>, PthreadHash, PthreadEqual> ptrs_of_alloc_thread;

    size_t borrowed_count;
};

static multi_worker_thread_pool m_thread_pool(4);
static worker                              worker;

static merge_bucket_t m_merge_bucket;

static pthread_key_t m_tsd_key;

static std::mutex        m_tsd_global_set_mutex;
static std::set<tsd_t *> m_tsd_global_set;

static std::mutex              m_merge_mutex;
static std::condition_variable m_merge_cv;

static std::list<tsd_t *> m_merge_waiting_buffer;

static std::shared_mutex                             m_dirty_ptrs_shared_mutex;
static std::unordered_set<void *, PtrHash, PtrEqual> m_dirty_ptrs; // 标记所有可能跨线程分配的指针

static std::atomic_uint64_t m_mem_stamp;

static size_t m_max_tsd_capacity    = 20000; // TODO JNI setter
static bool   is_stacktrace_enabled = false;

static bool   is_caller_sampling_enabled = false;
static size_t m_sample_size_min          = 0;

static size_t m_sample_size_max = 0;

static double m_sampling = 0.01;

// profile
static size_t m_minor_merge_times = 0;
static size_t m_full_merge_times  = 0;
static size_t m_dump_times        = 0;
static size_t m_lost_ptr_count    = 0;

static std::map<void *, stack_meta_t> m_debug_lost_ptr_stack; // fixme remove

void enable_stacktrace(bool __enable) {
    is_stacktrace_enabled = __enable;
}

void set_sample_size_range(size_t __min, size_t __max) {
    m_sample_size_min = __min;
    m_sample_size_max = __max;
}

void set_sampling(double __sampling) {
    m_sampling = __sampling;
}

void enable_caller_sampling(bool __enable) {
    is_caller_sampling_enabled = __enable;
}

static inline void
decrease_stack_size(std::map<uint64_t, stack_meta_t> &__stack_metas,
                    const ptr_meta_t &__ptr_meta) {
    LOGI(TAG, "calculate_stack_size");
    if (is_stacktrace_enabled
        && __ptr_meta.stack_hash
        && __stack_metas.count(__ptr_meta.stack_hash)) {
        LOGD(TAG, "into calculate_stack_size");
        stack_meta_t &stack_meta = __stack_metas.at(__ptr_meta.stack_hash);

        if (stack_meta.size > __ptr_meta.size) { // 减去同堆栈的 size
            stack_meta.size -= __ptr_meta.size;
        } else { // 删除 size 为 0 的堆栈
            delete stack_meta.p_stacktraces;
            __stack_metas.erase(__ptr_meta.stack_hash);
        }
    }
}

static inline void flush_tsd_to_bucket(tsd_t *__tsd_src, merge_bucket_t *__merge_bucket) {
    LOGI(TAG, "flush_tsd_to_bucket: enter");
    assert(__tsd_src != nullptr);
    assert(__merge_bucket != nullptr);

    NanoSeconds_Start(combine_ptr_begin);
    auto &ptr_of_thread = __merge_bucket->ptrs_of_alloc_thread[__tsd_src->owner_thread];

    // 保存分配记录
    for (auto &ptr_it : __tsd_src->ptr_metas) {
        auto &ptr      = ptr_it.first;
        auto &ptr_meta = ptr_it.second;

        ptr_of_thread.insert(ptr);

        auto &dest_set = __merge_bucket->ptr_metas[ptr];
        dest_set.insert(ptr_meta);
    }

    // 不同的 tsd 中可能有相同的堆栈, 累加 size
    for (auto stack_it : __tsd_src->stack_metas) {
        auto &merge_stack_meta         = __merge_bucket->stack_metas[stack_it.first];
        merge_stack_meta.size += stack_it.second.size;
        merge_stack_meta.p_stacktraces = stack_it.second.p_stacktraces;
    }

    // 保存所有的 borrow 指针
    for (auto &borrow_it : __tsd_src->borrowed_ptrs) {
        auto &ptr    = borrow_it.first;
        auto &stamps = borrow_it.second;

        auto &dest_set = __merge_bucket->borrowed_ptrs[ptr];
        dest_set.insert(stamps);
        __merge_bucket->borrowed_count++;
    }

    NanoSeconds_End(combine_ptr_cost, combine_ptr_begin);
    LOGI(TAG, "flush_tsd_to_bucket: combine ptr_meta costs: %lld", combine_ptr_cost);
}

static inline size_t repay_debt(merge_bucket_t *__merge_bucket) {
    LOGD(TAG, "repay_debt");
    NanoSeconds_Start(begin);
//    size_t total_count = __merge_bucket->borrowed_ptrs.size();
    size_t          repaid_count = 0;
    size_t          total_count  = 0;
    size_t          dirty_count  = m_dirty_ptrs.size();
    for (const auto &borrow_it : __merge_bucket->borrowed_ptrs) {
        auto &free_ptr    = borrow_it.first;
        auto &free_stamps = borrow_it.second; // set<uint64_t>
        total_count += free_stamps.size();

        auto alloc_it = __merge_bucket->ptr_metas.find(free_ptr);
        if (alloc_it == __merge_bucket->ptr_metas.end()) {
//            LOGD(TAG, "LOST: %p", free_ptr);
            std::lock_guard<std::shared_mutex> dirty_ptr_lock(m_dirty_ptrs_shared_mutex);
            m_dirty_ptrs.erase(free_ptr);
            continue;
        }

        auto &alloc_metas = alloc_it->second; // set<ptr_meta_t>

        auto free_stamp_it = free_stamps.begin();
        auto alloc_meta_it = alloc_metas.begin();

        //  TODO 推断优化 ？
        while (free_stamp_it != free_stamps.end() && alloc_meta_it != alloc_metas.end()) {
            if (*free_stamp_it < alloc_meta_it->mem_stamp) {
                free_stamp_it++;
                continue;
            }

            repaid_count++;
            free_stamp_it++;
            __merge_bucket->ptrs_of_alloc_thread[alloc_meta_it->owner_thread].erase(free_ptr);
            alloc_metas.erase(alloc_meta_it++);
        }

        if (alloc_metas.empty()) {
            __merge_bucket->ptr_metas.erase(free_ptr);
            std::lock_guard<std::shared_mutex> dirty_ptr_lock(m_dirty_ptrs_shared_mutex);
            m_dirty_ptrs.erase(free_ptr);
        } else if (alloc_metas.size() > 1) {
            // 可能 free 没有 hook 到? 打地鼠 case？不管怎样都只保留最后一个
            auto rbegin = alloc_metas.rbegin();
            rbegin++;
            assert(rbegin != alloc_metas.rend());
            alloc_metas.erase(alloc_metas.begin(), rbegin.base());
            assert(alloc_metas.size() == 1);
        }
    }

    m_lost_ptr_count += total_count - repaid_count;
    NanoSeconds_End(cost, begin);
    LOGD(TAG,
         "repay_debt cost %lld, current repaid count = %zu, current lost = %zu, dirty count = %zu -> %zu",
         cost, repaid_count, total_count - repaid_count, dirty_count, m_dirty_ptrs.size());
    return repaid_count;
}

static inline void minor_merge() {
    std::lock_guard<std::mutex>        global_tsd_set_lock(m_tsd_global_set_mutex);
    std::lock_guard<std::shared_mutex> merge_bucket_lock(m_merge_bucket.bucket_shared_mutex);

    for (auto tsd : m_merge_waiting_buffer) {
        {
            std::lock_guard<std::mutex> tsd_lock(tsd->tsd_mutex);
            flush_tsd_to_bucket(tsd, &m_merge_bucket);

            if (m_tsd_global_set.count(tsd)) {
                m_tsd_global_set.erase(tsd);
            } else {
                LOGE(TAG, "minor_merge: wtf tsd[%p] not found!", tsd);
                abort();
            }
        }
        // safe delete
        m_thread_pool.execute([tsd] {
            delete tsd;
        }, tsd->worker_idx);
    }
}

static inline size_t full_merge() {
    LOGI(TAG, "full_merge: enter");
    NanoSeconds_Start(full_begin);
    std::lock_guard<std::mutex>        global_tsd_set_lock(m_tsd_global_set_mutex);
    std::lock_guard<std::shared_mutex> merge_bucket_lock(m_merge_bucket.bucket_shared_mutex);
    size_t                             count = m_tsd_global_set.size();

    for (auto tsd : m_tsd_global_set) {
        std::lock_guard<std::mutex> tsd_lock(tsd->tsd_mutex);

        flush_tsd_to_bucket(tsd, &m_merge_bucket);
        tsd->ptr_metas.clear();
        tsd->stack_metas.clear();
        tsd->borrowed_ptrs.clear();
    }

    repay_debt(&m_merge_bucket);

    {
        std::map<void *, std::set<uint64_t>> temp_set;
        m_merge_bucket.borrowed_ptrs.swap(temp_set);
        m_merge_bucket.borrowed_count = 0;
        temp_set.clear();
    }

    NanoSeconds_End(full_cost, full_begin);
    LOGD(TAG, "full_merge: full merge cost: %lld", full_cost);
    return count;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

static void *merge_async(void *__arg) {

    while (true) {
        std::unique_lock<std::mutex> merge_lock(m_merge_mutex);
        while (m_merge_waiting_buffer.empty()) {
            LOGD(TAG, "merge_async: waiting...");
            m_merge_cv.wait(merge_lock);
            LOGD(TAG, "merge_async: proceed");
        }
        NanoSeconds_Start(merge_begin);

        minor_merge();
        m_minor_merge_times++;
        LOGD(TAG, "merge_async: retained borrowed count %zu", m_merge_bucket.borrowed_count);

        if (m_merge_bucket.borrowed_count > m_max_tsd_capacity * 10) {
            full_merge();
            m_full_merge_times++;
        }

        m_merge_waiting_buffer.clear();

        NanoSeconds_End(merge_cost, merge_begin);
        LOGD(TAG, "merge_async: merge cost: %lld", merge_cost);
    }
}

#pragma clang diagnostic pop

static inline void on_caller_thread_destroy(void *__arg) {
    LOGI(TAG, "on_caller_thread_destroy tid : %d", gettid());
    std::lock_guard<std::mutex>        global_tsd_set_lock(m_tsd_global_set_mutex);
    std::lock_guard<std::shared_mutex> merge_bucket_lock(m_merge_bucket.bucket_shared_mutex);

    auto current_tsd = static_cast<tsd_t *>(__arg);

    LOGI(TAG, "on_caller_thread_destroy: flush, tsd is %p", current_tsd);
    flush_tsd_to_bucket(current_tsd, &m_merge_bucket); // do we need a tsd lock?
//    LOGD(TAG, "on_caller_thread_destroy: repaid = %zu, retained = %zu, tsd = %p",
//         repaid_count, m_merge_bucket.borrowed_ptrs.size(), current_tsd);

    m_tsd_global_set.erase(current_tsd);

    m_thread_pool.execute([current_tsd] {
        delete current_tsd;
    }, current_tsd->worker_idx);

}

void memory_hook_init() {
    LOGI(TAG, "memory_hook_init");
    if (!m_tsd_key) {
        pthread_key_create(&m_tsd_key, on_caller_thread_destroy);
    }

    pthread_t merge_thread;
    pthread_create(&merge_thread, nullptr, merge_async, nullptr);
}

static inline tsd_t *tsd_fetch(bool __replace) {
    auto *tsd = (tsd_t *) pthread_getspecific(m_tsd_key);
    if (unlikely(!tsd) || unlikely(__replace)) {
        LOGI(TAG, "tsd_fetch: creating new tsd");
        tsd = new tsd_t;
        tsd->worker_idx = m_thread_pool.worker_choose(((uintptr_t)tsd >> 14) ^ ((unsigned long)tsd->owner_thread >> 4));
        pthread_setspecific(m_tsd_key, tsd);

        std::lock_guard<std::mutex> global_tsd_set_lock(m_tsd_global_set_mutex);

        m_tsd_global_set.insert(tsd);
    }
    assert(tsd != nullptr);
    return tsd;
}

static inline bool should_do_unwind(size_t __byte_count, void *__caller) {
    if (!is_caller_sampling_enabled) {

        return ((m_sample_size_min == 0 || __byte_count >= m_sample_size_min)
                && (m_sample_size_max == 0 || __byte_count <= m_sample_size_max)
                && rand() <= m_sampling * RAND_MAX);

    }

    // TODO caller sampling
    return false;
}

// fixme
static inline bool tsd_acquire(tsd_t *tsd, ptr_meta_t &__meta) {
//    tsd_t *tsd = tsd_fetch(false);

    std::lock_guard<std::mutex> tsd_lock(tsd->tsd_mutex);

    // TODO 只在 merge_bucket 里面存储 stack？
    // fixme 在锁外获取 Java 堆栈
    // fixme 不用 new vector
    if (is_stacktrace_enabled && should_do_unwind(__meta.size, __meta.caller)) {

        auto ptr_stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind(*ptr_stack_frames);

        if (!ptr_stack_frames->empty()) {
            uint64_t stack_hash = hash_stack_frames(*ptr_stack_frames);
            __meta.stack_hash = stack_hash;
            stack_meta_t &stack_meta = tsd->stack_metas[stack_hash];
            stack_meta.size += __meta.size;

            if (stack_meta.p_stacktraces) { // 相同的堆栈只记录一个
                delete ptr_stack_frames;
            } else {
                stack_meta.p_stacktraces = ptr_stack_frames;
            }
        } else {
            delete ptr_stack_frames;
        }
    }

    tsd->ptr_metas.emplace(__meta.ptr, __meta);

    bool trigger_merge = (tsd->ptr_metas.size() > m_max_tsd_capacity
                          || tsd->borrowed_ptrs.size() > m_max_tsd_capacity
                          || tsd->stack_metas.size() > m_max_tsd_capacity);

    return trigger_merge;
}

static inline void on_acquire_memory(void *__caller,
                                     void *__ptr,
                                     size_t __byte_count,
                                     bool __is_mmap) {
//    NanoSeconds_Start(alloc_begin);
    uint64_t stamp = m_mem_stamp.fetch_add(1, std::memory_order_release);
//    NanoSeconds_End(alloc_cost, alloc_begin);
//    LOGD(TAG, "alloc stamp cost %lld", alloc_cost);

    tsd_t *tsd = tsd_fetch(false);
    if (!__ptr) {
        LOGE(TAG, "on_acquire_memory: invalid pointer");
        return;
    }

    ptr_meta_t ptr_meta{};
    ptr_meta.ptr          = __ptr;
    ptr_meta.size         = __byte_count;
    ptr_meta.caller       = __caller;
    ptr_meta.stack_hash   = 0;
    ptr_meta.is_mmap      = __is_mmap;
    ptr_meta.mem_stamp    = stamp;
    ptr_meta.owner_thread = tsd->owner_thread;

//    m_thread_pool.execute([&tsd, &ptr_meta] {

    if (tsd_acquire(tsd, ptr_meta)) {

//        NanoSeconds_Start(begin);

        tsd_fetch(true); // replace current tsd fixme 如果跑在子线程的话不能在这里 fetch
        std::unique_lock<std::mutex> merge_lock(m_merge_mutex);
        m_merge_waiting_buffer.emplace_back(tsd);
        m_merge_cv.notify_one();

//        NanoSeconds_End(cost, begin);
        LOGD(TAG,
             "on_acquire_memory: triggerred flush ptr = %zu, bor = %zu, stack = %zu",
             tsd->ptr_metas.size(), tsd->borrowed_ptrs.size(), tsd->stack_metas.size());
    }

//    }, tsd->worker_idx);


//    NanoSeconds_End(alloc_cost, alloc_begin);
//    LOGD(TAG, "alloc cost %lld", alloc_cost);
}

static inline bool do_release_memory_meta(void *__ptr, tsd_t *__tsd, bool __is_mmap) {
//    auto meta_it = __tsd->ptr_metas.find(__ptr);
    auto meta_it = __tsd->ptr_metas.equal_range(__ptr);
    if (unlikely(meta_it.first == meta_it.second)) {
        return false;
    }

//    auto &ptr_set = meta_it->second;
    auto idx = meta_it.first;
    assert(++idx == meta_it.second);// 到这里说明肯定有且只有一个记录, fixme crash why ?

    auto &meta = meta_it.first->second;

    decrease_stack_size(__tsd->stack_metas, meta);

    __tsd->ptr_metas.erase(__ptr); // 到这里说明肯定有且只有一个记录, 直接删除

    return true;
}

static inline bool is_dirty_ptr(void *__ptr) {
//    return false;
    m_dirty_ptrs_shared_mutex.lock_shared();

    bool is_dirty = 0 != m_dirty_ptrs.count(__ptr);

    m_dirty_ptrs_shared_mutex.unlock_shared();

    return is_dirty;
}

static inline void on_release_memory(void *__ptr, bool __is_mmap) {
    if (!__ptr) {
        LOGE(TAG, "on_release_memory: invalid pointer");
        return;
    }
    NanoSeconds_Start(release_begin);
//    long long query_cost = 0;
//    long long dirty_cost = 0;
    uint64_t stamp = m_mem_stamp.fetch_add(1, std::memory_order_release);

    tsd_t *tsd = tsd_fetch(false);

    m_thread_pool.execute([tsd, __ptr, __is_mmap, stamp] {
        bool released = false;
        bool merged   = false;
        if (!is_dirty_ptr(__ptr)) {
            // 该指针没有跨线程, 或者第一次跨线程
            {
                std::lock_guard<std::mutex> tsd_lock(tsd->tsd_mutex);
                released = do_release_memory_meta(__ptr, tsd, __is_mmap);
            }

            if (!released) {
//            NanoSeconds_Start(query_begin);

                // fixme 慢了一倍多
                m_merge_bucket.bucket_shared_mutex.lock_shared();

                if (0 != m_merge_bucket.ptrs_of_alloc_thread.count(tsd->owner_thread)) {
                    merged =
                            0 !=
                            m_merge_bucket.ptrs_of_alloc_thread.at(tsd->owner_thread).count(__ptr);
//                LOGD(TAG, "merged size = %zu",
//                     m_merge_bucket.ptrs_of_alloc_thread.at(tsd->owner_thread).size());
                }
                m_merge_bucket.bucket_shared_mutex.unlock_shared();

//            NanoSeconds_End(query_end, query_begin);
//            query_cost = query_end;
            }
        }

        if (!released) { // 该指针跨线程了或者被合并了

            if (!merged) {
//            NanoSeconds_Start(dirty_begin);
                std::lock_guard<std::shared_mutex> dirty_ptr_lock(m_dirty_ptrs_shared_mutex);
                m_dirty_ptrs.emplace(__ptr); // mark dirty
//            NanoSeconds_End(dirty_end, dirty_begin);
//            dirty_cost = dirty_end;
            }

            std::lock_guard<std::mutex> tsd_lock(tsd->tsd_mutex);
            tsd->borrowed_ptrs.emplace(__ptr, stamp); // borrow
        }
    }, tsd->worker_idx);

//    m_thread_pool.execute([tsd] {
////        usleep(100000);
//        assert(tsd != nullptr);
//        LOGD(TAG, "do nothing but fetch tsd, %p", tsd);
//    });

    NanoSeconds_End(release_cost, release_begin);
    LOGD(TAG, "release cost %lld", release_cost);
//    LOGD(TAG, "release cost %lld, query cost %lld, dirty cost %lld", release_cost, query_cost, dirty_cost);
}

void on_alloc_memory(void *__caller, void *__ptr, size_t __byte_count) {
    on_acquire_memory(__caller, __ptr, __byte_count, false);
}

void on_free_memory(void *__ptr) {
    on_release_memory(__ptr, true);
}

void on_mmap_memory(void *__caller, void *__ptr, size_t __byte_count) {
    on_acquire_memory(__caller, __ptr, __byte_count, true);
}

void on_munmap_memory(void *__ptr) {
    on_release_memory(__ptr, true);
}

/**
 * 区分 native heap 和 mmap 的 caller 和 stack
 * @param __merge_bucket
 * @param __heap_caller_metas
 * @param __mmap_caller_metas
 * @param __heap_stack_metas
 * @param __mmap_stack_metas
 *
 * fixme 去掉中转逻辑？
 */
static inline void collect_metas(merge_bucket_t &__merge_bucket,
                                 std::multimap<void *, ptr_meta_t> &__ptr_metas,
                                 std::map<void *, caller_meta_t> &__heap_caller_metas,
                                 std::map<void *, caller_meta_t> &__mmap_caller_metas,
                                 std::map<uint64_t, stack_meta_t> &__heap_stack_metas,
                                 std::map<uint64_t, stack_meta_t> &__mmap_stack_metas) {
    LOGD(TAG, "collect_metas");
    for (const auto &it : __merge_bucket.ptr_metas) {
        auto ptr           = it.first;
        auto &ptr_meta_set = it.second;

        for (const auto &meta : ptr_meta_set) {
            auto &dest_caller_metas = meta.is_mmap ? __mmap_caller_metas : __heap_caller_metas;
            auto &dest_stack_metas  = meta.is_mmap ? __mmap_stack_metas : __heap_stack_metas;

            if (meta.caller) {
                caller_meta_t &caller_meta = dest_caller_metas[meta.caller];
                caller_meta.pointers.emplace(ptr);
                caller_meta.total_size += meta.size;
            }

            if (meta.stack_hash
                && __merge_bucket.stack_metas.count(meta.stack_hash)) {
                LOGD(TAG, "collect_metas: into collect stack");
                auto &stack_meta = dest_stack_metas[meta.stack_hash];
                stack_meta.p_stacktraces = __merge_bucket.stack_metas.at(
                        meta.stack_hash).p_stacktraces;
                stack_meta.size += meta.size;
            }

            __ptr_metas.emplace(ptr, meta);
        }

    }
    LOGD(TAG, "collect_metas done");
}

static inline void dump_callers(FILE *log_file,
                                const std::multimap<void *, ptr_meta_t> &ptr_metas,
                                std::map<void *, caller_meta_t> &caller_metas) {

    if (caller_metas.empty()) {
        LOGI(TAG, "dump_callers: nothing dump");
        return;
    }

    LOGD(TAG, "dump_callers: count = %zu", caller_metas.size());
    fprintf(log_file, "dump_callers: count = %zu\n", caller_metas.size());

    std::unordered_map<std::string, size_t>                   caller_alloc_size_of_so;
    std::unordered_map<std::string, std::map<size_t, size_t>> same_size_count_of_so;

//    LOGE("Yves.debug", "caller so begin");
    // 按 so 聚类
    for (auto &i : caller_metas) {
//        LOGE("Yves.debug", "so sum loop");
        auto caller      = i.first;
        auto caller_meta = i.second;

        Dl_info dl_info;
        dladdr(caller, &dl_info);

        if (!dl_info.dli_fname) {
            continue;
        }
        caller_alloc_size_of_so[dl_info.dli_fname] += caller_meta.total_size;

        // 按 size 聚类
        for (auto pointer : caller_meta.pointers) {
//            LOGE("Yves.debug", "size sum loop");
            if (ptr_metas.count(pointer)) {
                auto ptr_meta = ptr_metas.find(pointer);
                same_size_count_of_so[dl_info.dli_fname][ptr_meta->second.size]++;
            } else {
                // fixme why ?
//                LOGE("Yves.debug", "ptr_meta not found !!!");
            }
        }
    }

//    LOGE("Yves.debug", "begin sort");
    // 排序 so
    std::multimap<size_t, std::string> result_sort_by_size;

    std::transform(caller_alloc_size_of_so.begin(),
                   caller_alloc_size_of_so.end(),
                   std::inserter(result_sort_by_size, result_sort_by_size.begin()),
                   [](std::pair<std::string, size_t> src) {
                       return std::pair<size_t, std::string>(src.second, src.first);
                   });
//    LOGE("Yves.debug", "sort end");

    size_t    caller_total_size = 0;
    for (auto i                 = result_sort_by_size.rbegin();
         i != result_sort_by_size.rend(); ++i) {
        LOGD(TAG, "so = %s, caller alloc size = %zu", i->second.c_str(), i->first);
        fprintf(log_file, "caller alloc size = %10zu b, so = %s\n", i->first, i->second.c_str());

        caller_total_size += i->first;

        auto                                             count_of_size = same_size_count_of_so[i->second];
        std::multimap<size_t, std::pair<size_t, size_t>> result_sort_by_mul;
        std::transform(count_of_size.begin(),
                       count_of_size.end(),
                       std::inserter(result_sort_by_mul, result_sort_by_mul.begin()),
                       [](std::pair<size_t, size_t> src) {
                           return std::pair<size_t, std::pair<size_t, size_t >>(
                                   src.first * src.second,
                                   std::pair<size_t, size_t>(src.first, src.second));
                       });
        int                                              lines         = 20; // fixme hard coding
        LOGD(TAG, "top %d (size * count):", lines);
        fprintf(log_file, "top %d (size * count):\n", lines);

        for (auto sc = result_sort_by_mul.rbegin();
             sc != result_sort_by_mul.rend() && lines; ++sc, --lines) {
            auto size  = sc->second.first;
            auto count = sc->second.second;
            LOGD(TAG, "   size = %10zu b, count = %zu", size, count);
            fprintf(log_file, "   size = %10zu b, count = %zu\n", size, count);
        }
    }

    LOGD(TAG, "\n---------------------------------------------------");
    fprintf(log_file, "\n---------------------------------------------------\n");
    LOGD(TAG, "| caller total size = %zu b", caller_total_size);
    fprintf(log_file, "| caller total size = %zu b\n", caller_total_size);
    LOGD(TAG, "---------------------------------------------------\n");
    fprintf(log_file, "---------------------------------------------------\n\n");
}

static inline void dump_debug_stacks() {
    for (auto map_it : m_debug_lost_ptr_stack) {
        auto ptr        = map_it.first;
        auto stack_meta = map_it.second;

        LOGD(TAG, "LOST ptr %p, stack = ", ptr);
        for (auto &it : *stack_meta.p_stacktraces) {
            Dl_info stack_info = {nullptr};
            int     success    = dladdr((void *) it.pc, &stack_info);

            std::stringstream stack_builder;

            char *demangled_name = nullptr;
            if (success > 0) {
                int status = 0;
                demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0, &status);
            }

            stack_builder << "      | "
                          << "#pc " << std::hex << it.rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " ("
                          << (success && stack_info.dli_fname ? stack_info.dli_fname : "(null)")
                          << ")"
                          << std::endl;

            LOGD(TAG, "%s", stack_builder.str().c_str());

            if (demangled_name) {
                free(demangled_name);
            }
        }
    }

    m_debug_lost_ptr_stack.clear();
}

static inline void dump_stacks(FILE *log_file,
                               std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (stack_metas.empty()) {
        LOGI(TAG, "stacktrace: nothing dump");
        return;
    }

    LOGD(TAG, "dump_stacks: hash count = %zu", stack_metas.size());
    fprintf(log_file, "dump_stacks: hash count = %zu\n", stack_metas.size());

    for (auto &stack_meta :stack_metas) {
        LOGD(TAG, "hash %lu : stack.size = %zu, %p", stack_meta.first, stack_meta.second.size,
             stack_meta.second.p_stacktraces);
    }

    std::unordered_map<std::string, size_t>                                      stack_alloc_size_of_so;
    std::unordered_map<std::string, std::vector<std::pair<size_t, std::string>>> stacktrace_of_so;

    for (auto &stack_meta : stack_metas) {
        auto hash            = stack_meta.first;
        auto size            = stack_meta.second.size;
        auto stacktrace      = stack_meta.second.p_stacktraces;

        std::string       caller_so_name;
        std::stringstream stack_builder;
        for (auto         it = stacktrace->begin(); it != stacktrace->end(); ++it) {
            Dl_info stack_info = {nullptr};
            int     success    = dladdr((void *) it->pc, &stack_info);

            std::string so_name = std::string(success ? stack_info.dli_fname : "");

            char *demangled_name = nullptr;
            if (success > 0) {
                int status = 0;
                demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0, &status);
            }

            stack_builder << "      | "
                          << "#pc " << std::hex << it->rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " ("
                          << (success && stack_info.dli_fname ? stack_info.dli_fname : "(null)")
                          << ")"
                          << std::endl;

            if (demangled_name) {
                free(demangled_name);
            }

            // fixme hard coding
            if (/*so_name.find("com.tencent.mm") == std::string::npos ||*/
                    so_name.find("libwxperf.so") != std::string::npos ||
                    !caller_so_name.empty()) {
                continue;
            }

            caller_so_name = so_name;
            stack_alloc_size_of_so[caller_so_name] += size;
        }

        std::pair<size_t, std::string> stack_size_pair(size, stack_builder.str());
        stacktrace_of_so[caller_so_name].push_back(stack_size_pair);
    }

    // 排序
    for (auto i = stack_alloc_size_of_so.begin(); i != stack_alloc_size_of_so.end(); ++i) {
        auto so_name       = i->first;
        auto so_alloc_size = i->second;

        LOGD(TAG, "\nmalloc size of so (%s) : remaining size = %zu", so_name.c_str(),
             so_alloc_size);
        fprintf(log_file, "\nmalloc size of so (%s) : remaining size = %zu\n", so_name.c_str(),
                so_alloc_size);

        for (auto it_stack = stacktrace_of_so[so_name].begin();
             it_stack != stacktrace_of_so[so_name].end(); ++it_stack) {

            LOGD(TAG, "malloc size of the same stack = %zu\n stacktrace : \n%s",
                 it_stack->first,
                 it_stack->second.c_str());

            fprintf(log_file, "malloc size of the same stack = %zu\n stacktrace : \n%s\n",
                    it_stack->first,
                    it_stack->second.c_str());
        }
    }

}

static inline void dump_impl(FILE *log_file, bool enable_mmap_hook) {

    size_t tsd_count = full_merge();
    m_dump_times++;

    m_merge_bucket.bucket_shared_mutex.lock_shared();

    LOGD(TAG, "retained borrowed count: %zu", m_merge_bucket.borrowed_ptrs.size());

    std::multimap<void *, ptr_meta_t> ptr_metas; // fixme
    std::map<void *, caller_meta_t>   heap_caller_metas;
    std::map<void *, caller_meta_t>   mmap_caller_metas;
    std::map<uint64_t, stack_meta_t>  heap_stack_metas;
    std::map<uint64_t, stack_meta_t>  mmap_stack_metas;

    collect_metas(m_merge_bucket,
                  ptr_metas,
                  heap_caller_metas,
                  mmap_caller_metas,
                  heap_stack_metas,
                  mmap_stack_metas);

    // native heap allocation
    dump_callers(log_file, ptr_metas, heap_caller_metas);
    dump_stacks(log_file, heap_stack_metas);
    dump_debug_stacks();

    if (enable_mmap_hook) {
        // mmap allocation
        LOGD(TAG, "############################# mmap ###################################\n\n");
        fprintf(log_file,
                "############################# mmap ###################################\n\n");

//        dump_callers(log_file, m_merge_bucket.ptr_metas, mmap_caller_metas);
        dump_stacks(log_file, mmap_stack_metas);
    }

    fprintf(log_file,
            "tsd count = %zu, borrowed count = %zu, lost cout = %zu, minor = %zu, full = %zu, dump = %zu\n"
            "<void *, ptr_meta_t> ptr_meta [%zu * %zu = (%zu)]\n"
            "<uint64_t, stack_meta_t> stack_meta [%zu * %zu = (%zu)]\n"
            "<void *> borrowed_ptrs [%zu * %zu = (%zu)]\n\n",

            tsd_count, m_merge_bucket.borrowed_ptrs.size(), m_lost_ptr_count,
            m_minor_merge_times, m_full_merge_times, m_dump_times,

            sizeof(ptr_meta_t) + sizeof(void *), m_merge_bucket.ptr_metas.size(),
            (sizeof(ptr_meta_t) + sizeof(void *)) * m_merge_bucket.ptr_metas.size(),

            sizeof(stack_meta_t) + sizeof(uint64_t), m_merge_bucket.stack_metas.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * m_merge_bucket.stack_metas.size(),

            sizeof(void *), m_merge_bucket.borrowed_ptrs.size(),
            sizeof(void *) * m_merge_bucket.borrowed_ptrs.size());

    m_merge_bucket.bucket_shared_mutex.unlock_shared();
}

void dump(bool enable_mmap_hook, const char *path) {
    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");


    FILE *log_file = fopen(path, "w+");
    LOGD(TAG, "dump path = %s", path);
    if (!log_file) {
        LOGE(TAG, "open file failed");
        return;
    }

    dump_impl(log_file, enable_mmap_hook);

    fflush(log_file);
    fclose(log_file);

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> memory dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

extern const HookFunction HOOK_MALL_FUNCTIONS[];

void memory_hook_on_dlopen(const char *__file_name) {
    LOGD(TAG, "memory_hook_on_dlopen: file %s, h_malloc %p, h_realloc %p, h_free %p", __file_name,
         h_malloc, h_realloc, h_free);
    if (is_stacktrace_enabled) {
        unwindstack::update_maps();
    }
    srand((unsigned int) time(NULL));
}

