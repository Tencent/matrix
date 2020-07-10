//
// Created by Yves on 2020-03-11.
//

#include <dlfcn.h>
#include <unordered_map>
#include <StackTrace.h>
#include <cxxabi.h>
#include <sstream>
#include <iostream>
#include <xhook.h>
#include <cinttypes>
#include <regex>
#include <set>
#include <regex.h>
#include <Utils.h>
#include <fcntl.h>
#include "PthreadExt.h"
#include "PthreadHook.h"
#include "pthread.h"
#include "Log.h"
#include "JNICommon.h"
#include "cJSON.h"
#include "ReentrantPrevention.h"

#define ORIGINAL_LIB "libc.so"
#define TAG "PthreadHook"

#define THREAD_NAME_LEN 16

typedef void *(*pthread_routine_t)(void *);

struct pthread_meta_t {
    pid_t tid;
    char  *thread_name;
//    char  *parent_name;

    uint64_t hash;

    std::vector<unwindstack::FrameData> native_stacktrace;

    std::atomic<char *> java_stacktrace;

    pthread_meta_t() : tid(0),
                       thread_name(nullptr),
//                       parent_name(nullptr),
                       hash(0),
                       java_stacktrace(nullptr) {
    };

    ~pthread_meta_t() = default;

    pthread_meta_t(const pthread_meta_t &src) {
        tid               = src.tid;
        thread_name       = src.thread_name;
//        parent_name       = src.parent_name;
        hash              = src.hash;
        native_stacktrace = src.native_stacktrace;
        java_stacktrace.store(src.java_stacktrace.load(std::memory_order_acquire),
                              std::memory_order_release);
    }
};

typedef struct {
    pthread_routine_t origin_func;
    void              *origin_args;
}            routine_wrapper_t;

struct regex_wrapper {
    const char *regex_str;
    regex_t    regex;

    regex_wrapper(const char *regexStr, const regex_t &regex) : regex_str(regexStr), regex(regex) {}

    friend bool operator<(const regex_wrapper &left, const regex_wrapper &right) {
        return static_cast<bool>(strcmp(left.regex_str, right.regex_str));
    }
};

static std::recursive_mutex m_pthread_meta_mutex;

static std::map<pthread_t, pthread_meta_t> m_pthread_metas;
static std::set<pthread_t>                 m_filtered_pthreads;

static std::set<regex_wrapper> m_hook_thread_name_regex;

static pthread_key_t m_rp_key;

static std::mutex m_subroutine_mutex;
static std::condition_variable m_subroutine_cv;

static std::set<pthread_t> m_pthread_routine_flags;

static void on_pthread_destroy(void *__specific);

void pthread_hook_init() {
    LOGD(TAG, "pthread_hook_init");

    if (!m_rp_key) {
        pthread_key_create(&m_rp_key, on_pthread_destroy);
    }

    rp_init();
}

void add_hook_thread_name(const char *__regex_str) {
//    std::regex regex(__regex_str);
    regex_t regex;
    if (0 != regcomp(&regex, __regex_str, REG_NOSUB)) {
        LOGE("PthreadHook", "regex compiled error: %s", __regex_str);
        return;
    }
    size_t len          = strlen(__regex_str) + 1;
    char   *p_regex_str = static_cast<char *>(malloc(len));
    strncpy(p_regex_str, __regex_str, len);
    regex_wrapper w_regex(p_regex_str, regex);
    m_hook_thread_name_regex.insert(w_regex);
    LOGD(TAG, "parent name regex: %s -> %s, len = %zu", __regex_str, p_regex_str, len);
}

static bool test_match_thread_name(pthread_meta_t &__meta) {
    for (const auto &w : m_hook_thread_name_regex) {
        if (__meta.thread_name && 0 == regexec(&w.regex, __meta.thread_name, 0, NULL, 0)) {
            LOGD(TAG, "test_match_thread_name: %s matches regex %s", __meta.thread_name,
                 w.regex_str);
            return true;
        } else {
            LOGD(TAG, "test_match_thread_name: %s NOT matches regex %s", __meta.thread_name,
                 w.regex_str);
        }
    }
    return false;
}

static inline bool on_pthread_create_locked(const pthread_t __pthread, char *__java_stacktrace, pid_t __tid) {
    std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);

    if (m_pthread_metas.count(__pthread)) {
        LOGD(TAG, "on_pthread_create: thread already recorded");
        return false;
    }

    pthread_meta_t &meta = m_pthread_metas[__pthread];

    meta.tid = __tid;

    // 如果还没 setname, 此时拿到的是父线程的名字, 在 setname 的时候有一次更正机会, 否则继承父线程名字
    // 如果已经 setname, 那么此时拿到的就是当前创建线程的名字
    meta.thread_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));
    if (0 != pthread_getname_ext(__pthread, meta.thread_name, THREAD_NAME_LEN)) {
        char temp_name[THREAD_NAME_LEN];
        snprintf(temp_name, THREAD_NAME_LEN, "tid-%d", pthread_gettid_np(__pthread));
        strncpy(meta.thread_name, temp_name, THREAD_NAME_LEN);
    }

    LOGD(TAG, "on_pthread_create: pthread = %ld, thread name: %s", __pthread, meta.thread_name);

    if (test_match_thread_name(meta)) {
        m_filtered_pthreads.insert(__pthread);
    }

    uint64_t native_hash = 0;
    uint64_t java_hash   = 0;

    meta.native_stacktrace.reserve(16 * 2);
    unwindstack::do_unwind(meta.native_stacktrace);
    native_hash = hash_stack_frames(meta.native_stacktrace);

    if (__java_stacktrace) {
        meta.java_stacktrace.store(__java_stacktrace);
        java_hash = hash_str(__java_stacktrace);
        LOGD(TAG, "on_pthread_create: java hash = %lu", java_hash);
    }

    if (native_hash || java_hash) {
        meta.hash = hash_combine(native_hash, java_hash);
    }

    return true;
}

static void notify_routine(const pthread_t __pthread) {
    std::lock_guard<std::mutex> routine_lock(m_subroutine_mutex);

    m_pthread_routine_flags.emplace(__pthread);
    LOGD(TAG, "notify waiting count : %zu", m_pthread_routine_flags.size());

    m_subroutine_cv.notify_all();
}

// notice: 在父线程回调此函数
static void on_pthread_create(const pthread_t __pthread) {
    LOGD(TAG, "+++++++ on_pthread_create");

    pid_t tid  = pthread_gettid_np(__pthread);

    if (!rp_acquire()) {
        LOGD(TAG, "reentrant!!!");
        notify_routine(__pthread);
        return;
    }

    // 反射 Java 获取堆栈时加锁会造成死锁, 提前获取堆栈
    const size_t BUF_SIZE         = 1024;
    char         *java_stacktrace = static_cast<char *>(malloc(BUF_SIZE));
    if (java_stacktrace) {
        get_java_stacktrace(java_stacktrace, BUF_SIZE);
//        strncpy(java_stacktrace, " (fake stacktrace)", BUF_SIZE);
    }

    LOGD(TAG, "parent_tid: %d -> tid: %d", pthread_gettid_np(pthread_self()), tid);
    bool recorded = on_pthread_create_locked(__pthread, java_stacktrace, tid);

    if (!recorded && java_stacktrace) {
        free(java_stacktrace);
    }

    rp_release();
    notify_routine(__pthread);

    LOGD(TAG, "------ on_pthread_create end");
}

/**
 * ~~on_pthread_setname 有可能在 on_pthread_create 之前先执行~~
 * 在增加了 cond 之后, 必然后于 on_pthread_create 执行
 *
 * @param __pthread
 * @param __name
 */
static void on_pthread_setname(pthread_t __pthread, const char *__name) {
    if (NULL == __name) {
        LOGE(TAG, "setting name null");
        return;
    }

    const size_t name_len = strlen(__name);

    if (0 == name_len || name_len >= THREAD_NAME_LEN) {
        LOGE(TAG, "pthread name is illegal, just ignore. len(%s)", __name);
        return;
    }

    LOGD(TAG, "++++++++ pre on_pthread_setname tid: %d, %s", pthread_gettid_np(__pthread), __name);

    {
        std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);

        if (!m_pthread_metas.count(__pthread)) { // always false
            // 到这里说明没有回调 on_pthread_create, setname 对 on_pthread_create 是可见的
            auto lost_thread_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));
            pthread_getname_ext(__pthread, lost_thread_name, THREAD_NAME_LEN);
            LOGE(TAG,
                 "on_pthread_setname: pthread hook lost: {%s} -> {%s}, maybe on_create has not been called",
                 lost_thread_name, __name);
            free(lost_thread_name);

            return;
        }

        // 到这里说明 on_pthread_create 已经回调了, 需要修正并检查新的线程名是否 match 正则

        pthread_meta_t &meta = m_pthread_metas.at(__pthread);

        LOGD(TAG, "on_pthread_setname: %s -> %s, tid:%d", meta.thread_name, __name, meta.tid);

        assert(meta.thread_name != nullptr);
        strncpy(meta.thread_name, __name, THREAD_NAME_LEN);

        bool parent_match = m_filtered_pthreads.count(__pthread) != 0;

        // 如果新线程名不 match, 但父线程名 match, 说明需要从 filter 集合中移除
        if (!test_match_thread_name(meta) && parent_match) {
            m_filtered_pthreads.erase(__pthread);
            LOGD(TAG, "--------------------------");
            return;
        }

        // 如果新线程 match, 但父线程名不 match, 说明需要添加仅 filter 集合
        if (test_match_thread_name(meta) && !parent_match) {
            m_filtered_pthreads.insert(__pthread);
            LOGD(TAG, "--------------------------");
            return;
        }
    }

    // 否则, 啥也不干 (都 match, 都不 match)
    LOGD(TAG, "--------------------------");
}

static inline void before_routine_start() {
    LOGI(TAG, "before_routine_start");
    std::unique_lock<std::mutex> routine_lock(m_subroutine_mutex);

    pthread_t self_thread = pthread_self();

    m_subroutine_cv.wait(routine_lock, [&self_thread] {
        return m_pthread_routine_flags.count(self_thread);
    });

    LOGI(TAG, "before_routine_start: create ready, just continue, waiting count : %zu", m_pthread_routine_flags.size());

    m_pthread_routine_flags.erase(self_thread);
}


static inline void pthread_dump_impl(FILE *__log_file) {
    if (!__log_file) {
        LOGE(TAG, "open file failed");
        return;
    }

    for (auto &i: m_pthread_metas) {
        auto &meta = i.second;
        LOGD(TAG, "========> RETAINED PTHREAD { name : %s, tid: %d }", meta.thread_name, meta.tid);
        fprintf(__log_file, "========> RETAINED PTHREAD { name : %s, tid: %d }\n",
                meta.thread_name, meta.tid);
        std::stringstream stack_builder;

        if (meta.native_stacktrace.empty()) {
            continue;
        }

        LOGD(TAG, "native stacktrace:");
        fprintf(__log_file, "native stacktrace:\n");

        for (auto &p_frame : meta.native_stacktrace) {
            Dl_info stack_info;
            dladdr((void *) p_frame.pc, &stack_info);

            std::string so_name = std::string(stack_info.dli_fname);

            int  status          = 0;
            char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
                                                       &status);

            LOGD(TAG, "  #pc %"
                    PRIxPTR
                    " %s (%s)", p_frame.rel_pc,
                 demangled_name ? demangled_name : "(null)", stack_info.dli_fname);
            fprintf(__log_file, "  #pc %" PRIxPTR " %s (%s)\n", p_frame.rel_pc,
                    demangled_name ? demangled_name : "(null)", stack_info.dli_fname);

            free(demangled_name);
        }

        LOGD(TAG, "java stacktrace:\n%s", meta.java_stacktrace.load(std::memory_order_acquire));
        fprintf(__log_file, "java stacktrace:\n%s\n",
                meta.java_stacktrace.load(std::memory_order_acquire));
    }
}

void pthread_dump(const char *__path) {
    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);

    FILE *log_file = fopen(__path, "w+");
    LOGD(TAG, "pthread dump path = %s", __path);

    pthread_dump_impl(log_file);

    fclose(log_file);

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}


static inline char *pthread_dump_json_impl(FILE *__log_file) {

    LOGD(TAG, "pthread dump waiting count: %zu", m_pthread_routine_flags.size());

    std::map<uint64_t, std::vector<pthread_meta_t>> pthread_metas_by_hash;

    for (auto &i : m_filtered_pthreads) {
        auto &meta = m_pthread_metas[i];
        if (meta.hash) {
            auto &hash_bucket = pthread_metas_by_hash[meta.hash];
            hash_bucket.emplace_back(meta);
        }
    }

    char  *json_str    = NULL;
    cJSON *threads_arr = NULL;

    cJSON *json_obj = cJSON_CreateObject();

    if (!json_obj) {
        goto err;
    }

    threads_arr = cJSON_AddArrayToObject(json_obj, "PthreadHook");

    if (!threads_arr) {
        goto err;
    }

    for (auto &i : pthread_metas_by_hash) {
        auto &hash  = i.first;
        auto &metas = i.second;

        cJSON *hash_obj = cJSON_CreateObject();

        if (!hash_obj) {
            goto err;
        }

        cJSON_AddStringToObject(hash_obj, "hash", std::to_string(hash).c_str());
        assert(!metas.empty());

        std::stringstream stack_builder;
        for (auto         &frame : metas.front().native_stacktrace) {
            Dl_info stack_info = {nullptr};
            int     success    = dladdr((void *) frame.pc, &stack_info);

//            LOGE(TAG, "===> success = %d, pc =  %p, dl_info.dli_sname = %p %s",
//                 success, (void *) frame.pc, (void *) stack_info.dli_sname, stack_info.dli_sname);

            char *demangled_name = nullptr;
            if (success > 0) {
                int status = 0;
                demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0, &status);
            }

            stack_builder << "#pc " << std::hex << frame.rel_pc << " "
                          << (demangled_name ? demangled_name : "(null)")
                          << " ("
                          << (success && stack_info.dli_fname ? stack_info.dli_fname : "(null)")
                          << ");";

            if (demangled_name) {
                free(demangled_name);
            }
        }
        cJSON_AddStringToObject(hash_obj, "native", stack_builder.str().c_str());

        const char *java_stacktrace = metas.front().java_stacktrace.load(std::memory_order_acquire);
        cJSON_AddStringToObject(hash_obj, "java", java_stacktrace ? java_stacktrace : "");

        cJSON_AddStringToObject(hash_obj, "count", std::to_string(metas.size()).c_str());

        cJSON *same_hash_metas_arr = cJSON_AddArrayToObject(hash_obj, "threads");

        if (!same_hash_metas_arr) {
            goto err;
        }

        for (auto &meta: metas) {
            cJSON *meta_obj = cJSON_CreateObject();

            if (!meta_obj) {
                goto err;
            }

            cJSON_AddStringToObject(meta_obj, "tid", std::to_string(meta.tid).c_str());
            cJSON_AddStringToObject(meta_obj, "name", meta.thread_name);

            cJSON_AddItemToArray(same_hash_metas_arr, meta_obj);
        }

        cJSON_AddItemToArray(threads_arr, hash_obj);

        LOGD(TAG, "%s", cJSON_Print(hash_obj));
    }

    json_str = cJSON_PrintUnformatted(json_obj);

    cJSON_Delete(json_obj);

    fprintf(__log_file, "%s", json_str);
    return json_str;

    err:
    LOGD(TAG, "ERROR: create cJSON object failed");
    cJSON_Delete(json_obj);

    return nullptr;
}

void pthread_dump_json(const char *__path) {

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump json begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);

    FILE *log_file = fopen(__path, "w+");
    LOGD(TAG, "pthread dump path = %s", __path);

    if (log_file) {
        pthread_dump_json_impl(log_file);
        fclose(log_file);
    }

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump json end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

void pthread_hook_on_dlopen(const char *__file_name) {
    LOGD(TAG, "pthread_hook_on_dlopen");
    std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);
    unwindstack::update_maps();
    LOGD(TAG, "pthread_hook_on_dlopen end");
}

static void on_pthread_destroy(void *__specific) {
    LOGD(TAG, "on_pthread_destroy++++");
    std::lock_guard<std::recursive_mutex> meta_lock(m_pthread_meta_mutex);

    pthread_t destroying_thread = pthread_self();

    if (!m_pthread_metas.count(destroying_thread)) {
        LOGD(TAG, "on_pthread_destroy: thread not found");
        return;
    }

    pthread_meta_t &meta = m_pthread_metas.at(destroying_thread);
    LOGD(TAG, "removing thread {%ld, %s, %d}", destroying_thread, meta.thread_name, meta.tid);

    free(meta.thread_name);

    char *java_stacktrace = meta.java_stacktrace.load(std::memory_order_acquire);
    if (java_stacktrace) {
        free(java_stacktrace);
    }

    m_pthread_metas.erase(destroying_thread);
    m_filtered_pthreads.erase(destroying_thread);

    LOGD(TAG, "__specific %c", *(char *) __specific);

    free(__specific);

    LOGD(TAG, "on_pthread_destroy end----");
}

static void *pthread_routine_wrapper(void *__arg) {

    auto *specific = (char *) malloc(sizeof(char));
    *specific = 'P';

    pthread_setspecific(m_rp_key, specific);

    before_routine_start();

    auto *args_wrapper = (routine_wrapper_t *) __arg;
    void *ret          = args_wrapper->origin_func(args_wrapper->origin_args);
    free(args_wrapper);

    return ret;
}

DEFINE_HOOK_FUN(int, pthread_create, pthread_t *__pthread_ptr, pthread_attr_t const *__attr,
                void *(*__start_routine)(void *), void *__arg) {
    auto *args_wrapper = (routine_wrapper_t *) malloc(sizeof(routine_wrapper_t));
    args_wrapper->origin_func = __start_routine;
    args_wrapper->origin_args = __arg;

    CALL_ORIGIN_FUNC_RET(int, ret, pthread_create, __pthread_ptr, __attr, pthread_routine_wrapper,
                         args_wrapper);

    if (0 == ret) {
        on_pthread_create(*__pthread_ptr);
    }

    return ret;
}

DEFINE_HOOK_FUN(int, pthread_setname_np, pthread_t
        __pthread, const char *__name) {
    CALL_ORIGIN_FUNC_RET(int, ret, pthread_setname_np, __pthread, __name);
    if (0 == ret) {
        on_pthread_setname(__pthread, __name);
    }
    return ret;
}

#undef ORIGINAL_LIB
