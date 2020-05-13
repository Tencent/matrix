//
// Created by Yves on 2019-08-08.
//

#include <malloc.h>
#include <unistd.h>
#include <unordered_map>
#include <set>
#include <random>
#include <xhook.h>
#include <sstream>
#include <cxxabi.h>
#include <sys/mman.h>
#include "MemoryHookFunctions.h"
#include "StackTrace.h"
#include "Utils.h"
#include "unwindstack/Unwinder.h"
#include "MemoryHook.h"

// TODO protobuf
struct ptr_meta_t {
    size_t   size;
    void     *caller;
    uint64_t stack_hash;
    bool     is_mmap;
};

struct caller_meta_t {
    size_t           total_size;
    std::set<void *> pointers;
};

struct stack_meta_t {
    size_t                              size;
    std::vector<unwindstack::FrameData> *p_stacktraces;
};

struct tsd_t {
    std::multimap<void *, ptr_meta_t> ptr_meta;
    std::map<uint64_t, stack_meta_t>  stack_meta;
    std::multiset<void *>             borrowed_ptrs;
};

static tsd_t            m_merge_bucket;
static pthread_rwlock_t m_tsd_merge_bucket_lock;

static pthread_key_t m_tsd_key;

static std::set<tsd_t *> m_tsd_global_set;
static pthread_mutex_t   m_tsd_global_set_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool is_stacktrace_enabled      = false;
static bool is_caller_sampling_enabled = false;

static size_t m_sample_size_min = 0;
static size_t m_sample_size_max = 0;

static double m_sampling = 1;

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
decrease_stack_size(std::map<uint64_t, stack_meta_t> &__stack_metas, ptr_meta_t &__ptr_meta) {
    LOGI(TAG, "calculate_stack_size");
    if (is_stacktrace_enabled
        && __ptr_meta.stack_hash
        && __stack_metas.count(__ptr_meta.stack_hash)) {

        stack_meta_t &stack_meta = __stack_metas.at(__ptr_meta.stack_hash);

        if (stack_meta.size > __ptr_meta.size) { // 减去同堆栈的 size
            stack_meta.size -= __ptr_meta.size;
        } else { // 删除 size 为 0 的堆栈
            delete stack_meta.p_stacktraces;
            __stack_metas.erase(__ptr_meta.stack_hash);
        }
    }
}

static inline void flush_tsd_to(tsd_t *__tsd_src, tsd_t *__dest) {
    LOGI(TAG, "flush_tsd");

    // 不同的 tsd 中可能存在相同的指针, 只有一个应该被最终保留, 但为了所有 borrow 的指针可以正常对账, 先全部保存
    __dest->ptr_meta.insert(__tsd_src->ptr_meta.begin(), __tsd_src->ptr_meta.end());

    // 不同的 tsd 中可能有相同的堆栈, 累加 size
    for (auto it : __tsd_src->stack_meta) {
        auto &stack_meta         = __dest->stack_meta[it.first];
        stack_meta.size += it.second.size;
        stack_meta.p_stacktraces = it.second.p_stacktraces;
    }

    // 保存所有的 borrow 指针
    __dest->borrowed_ptrs.insert(__tsd_src->borrowed_ptrs.begin(), __tsd_src->borrowed_ptrs.end());

    // 还债
    for (auto borrow_it = __dest->borrowed_ptrs.begin();
         borrow_it != __dest->borrowed_ptrs.end();) {
        LOGI(TAG, "flush ptr %p", *borrow_it);
        if (!__dest->ptr_meta.count(*borrow_it)) {
            LOGI(TAG, "but ptr not found %p", *borrow_it);
            borrow_it++;
            continue;
        }

        // 有一还一
        auto ptr_meta_it = __dest->ptr_meta.find(*borrow_it);
        if (ptr_meta_it != __dest->ptr_meta.end()) {
            decrease_stack_size(__dest->stack_meta, ptr_meta_it->second);
            __dest->ptr_meta.erase(ptr_meta_it);
            __dest->borrowed_ptrs.erase(borrow_it++);// note: 遍历时删除元素需要这样自增
        } else {
            borrow_it++;
        }
    }

    LOGI(TAG, "flush_tsd done");
}

void on_caller_thread_destroy(void *__arg) {
    LOGI(TAG, "on_caller_thread_destroy");
    pthread_mutex_lock(&m_tsd_global_set_mutex);
    tsd_t *current_tsd = static_cast<tsd_t *>(__arg);
    m_tsd_global_set.erase(current_tsd);

    pthread_rwlock_wrlock(&m_tsd_merge_bucket_lock);
    flush_tsd_to(current_tsd, &m_merge_bucket);
    pthread_rwlock_unlock(&m_tsd_merge_bucket_lock);

    delete current_tsd;
    pthread_mutex_unlock(&m_tsd_global_set_mutex);
}

void memory_hook_init() {
    LOGI(TAG, "memory_hook_init");
    if (!m_tsd_key) {
        pthread_key_create(&m_tsd_key, on_caller_thread_destroy);
    }
    pthread_mutex_lock(&m_tsd_global_set_mutex);
    m_tsd_global_set.insert(&m_merge_bucket);
    pthread_mutex_unlock(&m_tsd_global_set_mutex);
}

tsd_t *tsd_fetch() {
    auto *tsd = (tsd_t *) pthread_getspecific(m_tsd_key);
    if (unlikely(!tsd)) {
        pthread_mutex_lock(&m_tsd_global_set_mutex);

        tsd = new tsd_t;
        pthread_setspecific(m_tsd_key, tsd);
        m_tsd_global_set.insert(tsd);

        pthread_mutex_unlock(&m_tsd_global_set_mutex);
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

static inline void on_acquire_memory(void *__caller,
                                     void *__ptr,
                                     size_t __byte_count,
                                     bool __is_mmap) {

    tsd_t *__tsd = tsd_fetch();

    if (!__ptr) {
        LOGE(TAG, "on_acquire_memory: invalid pointer");
        return;
    }

    // 如果当前 tsd 指针重复了, 该指针要么在其他线程释放了, 要么释放函数没有 hook 到, 但记录都应该被清除
    __tsd->ptr_meta.erase(__ptr);

    ptr_meta_t ptr_meta;
    ptr_meta.size       = __byte_count;
    ptr_meta.caller     = __caller;
    ptr_meta.stack_hash = 0; // clear hash
    ptr_meta.is_mmap    = __is_mmap;

    if (is_stacktrace_enabled && should_do_unwind(__byte_count, __caller)) {

        auto ptr_stack_frames = new std::vector<unwindstack::FrameData>;
        unwindstack::do_unwind(*ptr_stack_frames);

        if (!ptr_stack_frames->empty()) {
            uint64_t stack_hash = hash_stack_frames(*ptr_stack_frames);
            ptr_meta.stack_hash = stack_hash;
            stack_meta_t &stack_meta = __tsd->stack_meta[stack_hash];
            stack_meta.size += __byte_count;

            if (stack_meta.p_stacktraces) { // 相同的堆栈只记录一个
                delete ptr_stack_frames;
            } else {
                stack_meta.p_stacktraces = ptr_stack_frames;
            }
        } else {
            delete ptr_stack_frames;
        }
    }

    __tsd->ptr_meta.emplace(__ptr, ptr_meta);

    // todo countdown to flush
}

static inline bool do_release_memory_meta(void *__ptr, tsd_t *__tsd, bool __is_mmap) {
    // 单个线程内不存在多个相同的指针
    auto meta_it = __tsd->ptr_meta.find(__ptr);
    if (unlikely(meta_it == __tsd->ptr_meta.end())) {
        return false;
    }

    auto &ptr_meta = meta_it->second;

    decrease_stack_size(__tsd->stack_meta, ptr_meta);

    __tsd->ptr_meta.erase(__ptr);

    return true;
}

static inline void on_release_memory(void *__ptr, bool __is_mmap) {
    if (!__ptr) {
        LOGE(TAG, "on_release_memory: invalid pointer");
        return;
    }

    tsd_t *tsd = tsd_fetch();

    bool released = do_release_memory_meta(__ptr, tsd, __is_mmap);

    if (unlikely(!released)) {
        pthread_rwlock_rdlock(&m_tsd_merge_bucket_lock);
        released = do_release_memory_meta(__ptr, &m_merge_bucket, __is_mmap);
        pthread_rwlock_unlock(&m_tsd_merge_bucket_lock);
    }

    if (unlikely(!released)) { // free 的指针没记录或者还在某个线程的 tsd 里没有 flush, 可能有多个相同的
        tsd->borrowed_ptrs.insert(__ptr);
    }

    // TODO countdown to flush
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
 * @param __tsd
 * @param __heap_caller_metas
 * @param __mmap_caller_metas
 * @param __heap_stack_metas
 * @param __mmap_stack_metas
 */
static inline void collect_metas(tsd_t &__tsd,
                                 std::map<void *, caller_meta_t> &__heap_caller_metas,
                                 std::map<void *, caller_meta_t> &__mmap_caller_metas,
                                 std::map<uint64_t, stack_meta_t> &__heap_stack_metas,
                                 std::map<uint64_t, stack_meta_t> &__mmap_stack_metas) {

    for (auto it : __tsd.ptr_meta) {
        auto &ptr_meta          = it.second;
        auto ptr                = it.first;
        auto &dest_caller_metas = ptr_meta.is_mmap ? __mmap_caller_metas : __heap_caller_metas;
        auto &dest_stack_metas  = ptr_meta.is_mmap ? __mmap_stack_metas : __heap_stack_metas;

        if (ptr_meta.caller) {
            caller_meta_t &caller_meta = dest_caller_metas[ptr_meta.caller];
            caller_meta.pointers.emplace(ptr);
            caller_meta.total_size += ptr_meta.size;
        }

        if (ptr_meta.stack_hash
            && __tsd.stack_meta.count(ptr_meta.stack_hash)) {
            auto &stack_meta = dest_stack_metas[ptr_meta.stack_hash];
            stack_meta.p_stacktraces = __tsd.stack_meta.at(ptr_meta.stack_hash).p_stacktraces;
            stack_meta.size += ptr_meta.size;
        }
    }
}

static inline void dump_callers(FILE *log_file,
                                std::multimap<void *, ptr_meta_t> &ptr_metas,
                                std::map<void *, caller_meta_t> &caller_metas) {

    if (caller_metas.empty()) {
        LOGI(TAG, "caller: nothing dump");
        return;
    }

    LOGD(TAG, "caller count = %zu", caller_metas.size());
    fprintf(log_file, "caller count = %zu\n", caller_metas.size());

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

static inline void dump_stacks(FILE *log_file,
                               std::map<uint64_t, stack_meta_t> &stack_metas) {
    if (stack_metas.empty()) {
        LOGI(TAG, "stacktrace: nothing dump");
        return;
    }

    LOGD(TAG, "hash count = %zu", stack_metas.size());
    fprintf(log_file, "hash count = %zu\n", stack_metas.size());

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

    pthread_mutex_lock(&m_tsd_global_set_mutex);

    tsd_t dump_dst;

    size_t tsd_size = m_tsd_global_set.size();

    for (auto tsd : m_tsd_global_set) {
        flush_tsd_to(tsd, &dump_dst);
    }

    pthread_mutex_unlock(&m_tsd_global_set_mutex);

    LOGD(TAG, "retained borrowed count: %zu", dump_dst.borrowed_ptrs.size());

    std::map<void *, caller_meta_t>  heap_caller_metas;
    std::map<void *, caller_meta_t>  mmap_caller_metas;
    std::map<uint64_t, stack_meta_t> heap_stack_metas;
    std::map<uint64_t, stack_meta_t> mmap_stack_metas;

    collect_metas(dump_dst,
                  heap_caller_metas,
                  mmap_caller_metas,
                  heap_stack_metas,
                  mmap_stack_metas);

    // native heap allocation
    dump_callers(log_file, dump_dst.ptr_meta, heap_caller_metas);
    dump_stacks(log_file, heap_stack_metas);

    fprintf(log_file,
            "tsd count = %zu\n"
            "<void *, ptr_meta_t> m_ptr_meta [%zu * %zu = (%zu)]\n"
            "<void *, caller_meta_t> m_caller_meta [%zu * %zu = (%zu)]\n"
            "<uint64_t, stack_meta_t> m_stack_meta [%zu * %zu = (%zu)]\n\n",

            tsd_size,

            sizeof(ptr_meta_t) + sizeof(void *), dump_dst.ptr_meta.size(),
            (sizeof(ptr_meta_t) + sizeof(void *)) * dump_dst.ptr_meta.size(),

            sizeof(caller_meta_t) + sizeof(void *), heap_caller_metas.size(),
            (sizeof(caller_meta_t) + sizeof(void *)) * heap_caller_metas.size(),

            sizeof(stack_meta_t) + sizeof(uint64_t), heap_stack_metas.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * heap_stack_metas.size());

    if (!enable_mmap_hook) {
        return;
    }

    // mmap allocation
    LOGD(TAG, "############################# mmap ###################################\n\n");
    fprintf(log_file, "############################# mmap ###################################\n\n");

    dump_callers(log_file, dump_dst.ptr_meta, mmap_caller_metas);
    dump_stacks(log_file, mmap_stack_metas);

    fprintf(log_file,
            "tsd count = %zu\n"
            "<void *, ptr_meta_t> m_ptr_meta [%zu * %zu = (%zu)]\n"
            "<void *, caller_meta_t> m_caller_meta [%zu * %zu = (%zu)]\n"
            "<uint64_t, stack_meta_t> m_stack_meta [%zu * %zu = (%zu)]\n\n",

            tsd_size,

            sizeof(ptr_meta_t) + sizeof(void *), dump_dst.ptr_meta.size(),
            (sizeof(ptr_meta_t) + sizeof(void *)) * dump_dst.ptr_meta.size(),
            sizeof(caller_meta_t) + sizeof(void *), mmap_caller_metas.size(),
            (sizeof(caller_meta_t) + sizeof(void *)) * mmap_caller_metas.size(),
            sizeof(stack_meta_t) + sizeof(uint64_t), mmap_stack_metas.size(),
            (sizeof(stack_meta_t) + sizeof(uint64_t)) * mmap_stack_metas.size());
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

void memory_hook_on_dlopen(const char *__file_name) {
    LOGD(TAG, "memory_hook_on_dlopen");
    if (is_stacktrace_enabled) {
        unwindstack::update_maps();
    }
    srand((unsigned int) time(NULL));
}

