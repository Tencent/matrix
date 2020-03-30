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
#include "PthreadHook.h"
#include "pthread.h"
#include "log.h"
#include "JNICommon.h"

#define ORIGINAL_LIB "libc.so"
#define TAG "PthreadHook"

#define THREAD_NAME_LEN 16

typedef void *(*pthread_routine_t)(void *);

extern "C" typedef struct {
    pthread_t pthread; // key ?
    pid_t     tid;
    char      *thread_name;
    char      *parent_name;

    std::atomic<char *> java_stacktrace;

    std::atomic<std::vector<unwindstack::FrameData> *> native_stacktrace;
}            pthread_meta_t;

extern "C" typedef struct {
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

static pthread_mutex_t     m_pthread_meta_mutex;
static pthread_mutexattr_t attr;

static pthread_mutex_t m_unwind_mutex = PTHREAD_MUTEX_INITIALIZER;


static std::unordered_map<pthread_t, pthread_meta_t> m_pthread_metas;
static std::set<regex_wrapper>                       m_hook_thread_name_regex;

static pthread_key_t m_key;
//static pthread_cond_t m_wrapper_cond = PTHREAD_COND_INITIALIZER;

void pthread_hook_init() {
    LOGD(TAG, "pthread_hook_init");
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m_pthread_meta_mutex, &attr);
}

void add_hook_thread_name(const char *__regex_str) {
//    std::regex regex(__regex_str);
    regex_t regex;
    if (0 != regcomp(&regex, __regex_str, REG_NOSUB)) {
        LOGE("PthreadHook", "regex compiled error: %s", __regex_str);
        return;
    }
    size_t len          = sizeof(__regex_str) + sizeof(char);
    char   *p_regex_str = static_cast<char *>(malloc(len));
    strncpy(p_regex_str, __regex_str, len);
    regex_wrapper w_regex(p_regex_str, regex);
    m_hook_thread_name_regex.insert(w_regex);
    LOGD(TAG, "parent name regex: %s", __regex_str);
}

static int read_thread_name(pthread_t __pthread, char *__buf, size_t __n) {
    if (!__buf) {
        return -1;
    }

    char proc_path[128];

    sprintf(proc_path, "/proc/self/task/%d/stat", pthread_gettid_np(__pthread));

    FILE *file = fopen(proc_path, "r");

    if (!file) {
        LOGD(TAG, "file not found: %s", proc_path);
        return -1;
    }

    fscanf(file, "%*d (%[^)]", __buf);

    LOGD(TAG, "read thread name %s, len %zu", __buf, strlen(__buf));

    fclose(file);

    return 0;
}

inline int wrap_pthread_getname_np(pthread_t __pthread, char *__buf, size_t __n) {
#if __ANDROID_API__ >= 26
    return pthread_getname_np(__pthread, __buf, __n);
#else
    return read_thread_name(__pthread, __buf, __n);
#endif
}

static bool should_do_unwind(pthread_meta_t &__meta) {
    for (const auto &w : m_hook_thread_name_regex) {
        if (0 == regexec(&w.regex, __meta.thread_name, 0, NULL, 0)) {
            LOGD(TAG, "%s matches regex %s", __meta.thread_name, w.regex_str);
            return true;
        }
    }
    return false;
}


static void unwind_pthread_stacktrace(pthread_meta_t &__meta) {
    // do unwind
//    为什么这样写会挂？ *dst = unwinder.frames() 时 Fault Address
//    __meta.native_stacktrace = new std::vector<unwindstack::FrameData>;
    auto *tmp_ns= new std::vector<unwindstack::FrameData>;
    unwindstack::do_unwind(tmp_ns);
    __meta.native_stacktrace = tmp_ns;

    __meta.java_stacktrace = static_cast<char *>(malloc(sizeof(char) * 1024));
    get_java_stacktrace(__meta.java_stacktrace, 1024);
}

static void on_pthread_create(const pthread_t __pthread_ptr) {

    pthread_t pthread = __pthread_ptr;
    pid_t     tid     = pthread_gettid_np(pthread);

    LOGD(TAG, "on_pthread_create tid: %d", tid);
    pthread_mutex_lock(&m_pthread_meta_mutex);

    if (m_pthread_metas.count(pthread)) {
        LOGD(TAG, "on_pthread_create: thread already recorded");
        pthread_mutex_unlock(&m_pthread_meta_mutex);
        return;
    }

    pthread_meta_t &meta = m_pthread_metas[pthread];

    meta.tid = tid;

    char *parent_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));

    if (wrap_pthread_getname_np(pthread, parent_name, THREAD_NAME_LEN) == 0) {
    } else {
        strncpy(parent_name, "(null)", THREAD_NAME_LEN);
    }

    meta.thread_name = meta.parent_name = parent_name; // 子线程会继承父线程名

    LOGD(TAG, "pthread = %ld, parent name: %s, thread name: %s", pthread, meta.parent_name,
         meta.thread_name);

    std::atomic_bool need_unwind = should_do_unwind(meta);

    pthread_mutex_unlock(&m_pthread_meta_mutex);

    if (need_unwind) {
        unwind_pthread_stacktrace(meta);
    }

    LOGD(TAG, "on_pthread_create end");
}

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

    LOGD(TAG, "pre on_pthread_setname tid: %d, %s", pthread_gettid_np(__pthread), __name);

    pthread_mutex_lock(&m_pthread_meta_mutex);

    if (!m_pthread_metas.count(__pthread)) {
        auto lost_thread_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));
        wrap_pthread_getname_np(__pthread, lost_thread_name, THREAD_NAME_LEN);
        LOGE(TAG, "pthread hook lost: {%s}", lost_thread_name);
        free(lost_thread_name);
        pthread_mutex_unlock(&m_pthread_meta_mutex);
        return;
    }

    pthread_meta_t &meta = m_pthread_metas.at(__pthread);

    LOGD(TAG, "on_pthread_setname: %s -> %s, tid:%d", meta.thread_name, __name, meta.tid);

    meta.thread_name = static_cast<char *>(malloc(sizeof(char) * (THREAD_NAME_LEN + 1)));
    strncpy(meta.thread_name, __name, THREAD_NAME_LEN + 1);

//    bool need_unwind = (!meta.native_stacktrace || meta.native_stacktrace->empty())
//                       && !meta.java_stacktrace
//                       && ((meta.parent_name == NULL && meta.thread_name == NULL)
//                           || 0 != strncmp(meta.parent_name, meta.thread_name, THREAD_NAME_LEN));

    // 子线程与父线程名不一致时，并且新线程名 match 正则，则需要 unwind
    std::atomic_bool need_unwind =
                             (0 != strncmp(meta.parent_name, meta.thread_name, THREAD_NAME_LEN)
                              && should_do_unwind(meta));

    pthread_mutex_unlock(&m_pthread_meta_mutex);

    if (need_unwind && !meta.native_stacktrace && !meta.java_stacktrace) {
        LOGD(TAG, "unwinding when setname");
        unwind_pthread_stacktrace(meta);
    }

    LOGD(TAG, "--------------------------");
}


void pthread_dump_impl(FILE *__log_file, std::unordered_map<pthread_t, pthread_meta_t> &metas) {
    if (!__log_file) {
        LOGE(TAG, "open file failed");
        return;
    }

    for (auto &i: metas) {
        auto &meta = i.second;
        LOGD(TAG, "========> RETAINED PTHREAD { name : %s, parent: %s, tid: %d }", meta.thread_name,
             meta.parent_name, meta.tid);
        fprintf(__log_file, "========> RETAINED PTHREAD { name : %s, parent: %s, tid: %d }\n",
                meta.thread_name, meta.parent_name, meta.tid);
        std::stringstream stack_builder;

        if (!meta.native_stacktrace) {
            continue;
        }

        LOGD(TAG, "native stacktrace:");
        fprintf(__log_file, "native stacktrace:\n");

        auto native_stacktrace = std::atomic_load(&meta.native_stacktrace);
        if (!native_stacktrace) {
            continue;
        }

        for (auto p_frame = native_stacktrace->begin();
             p_frame != native_stacktrace->end(); ++p_frame) {
            Dl_info stack_info;
            dladdr((void *) p_frame->pc, &stack_info);

            std::string so_name = std::string(stack_info.dli_fname);

            int  status          = 0;
            char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
                                                       &status);

            LOGD(TAG, "  #pc %"
                    PRIxPTR
                    " %s (%s)", p_frame->rel_pc,
                 demangled_name ? demangled_name : "(null)", stack_info.dli_fname);
            fprintf(__log_file, "  #pc %" PRIxPTR " %s (%s)\n", p_frame->rel_pc,
                    demangled_name ? demangled_name : "(null)", stack_info.dli_fname);

            free(demangled_name);
        }

        LOGD(TAG, "java stacktrace:\n%s", std::atomic_load(&meta.java_stacktrace));
        fprintf(__log_file, "java stacktrace:\n%s\n", std::atomic_load(&meta.java_stacktrace));
    }
}

void pthread_dump(const char *__path) {
    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    pthread_mutex_lock(&m_pthread_meta_mutex);

    FILE *log_file = fopen(__path, "w+");
    LOGD(TAG, "pthread dump path = %s", __path);

    pthread_dump_impl(log_file, m_pthread_metas);

    fclose(log_file);

    pthread_mutex_unlock(&m_pthread_meta_mutex);

    LOGD(TAG,
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

void pthread_hook_on_dlopen(const char *__file_name) {
    LOGD(TAG, "pthread_hook_on_dlopen");
    unwindstack::update_maps();

    LOGD(TAG, "pthread_hook_on_dlopen end");
}

void on_pthread_destroy(void *__specific) {
    LOGD(TAG, "on_pthread_destroy");
    pthread_mutex_lock(&m_pthread_meta_mutex);

    pthread_t destroying_thread = pthread_self();

    if (!m_pthread_metas.count(destroying_thread)) {
        LOGD(TAG, "on_pthread_destroy: thread not found");
        pthread_mutex_unlock(&m_pthread_meta_mutex);
        return;
    }

    pthread_meta_t &meta = m_pthread_metas.at(destroying_thread);
    LOGD(TAG, "removing thread {%ld, %s, %s, %d}", destroying_thread, meta.thread_name,
         meta.parent_name, meta.tid);

    if (meta.thread_name == meta.parent_name) {
        free(meta.thread_name);
    } else {
        free(meta.thread_name);
        free(meta.parent_name);
    }
    if (meta.java_stacktrace) {
        free(meta.java_stacktrace);
    }
    delete meta.native_stacktrace;

    meta.thread_name       = NULL;
    meta.parent_name       = NULL;
    meta.java_stacktrace   = NULL;
    meta.native_stacktrace = NULL;

    m_pthread_metas.erase(destroying_thread);
    pthread_mutex_unlock(&m_pthread_meta_mutex);

    LOGD(TAG, "__specific %c", *(char *) __specific);

    free(__specific);


    LOGD(TAG, "on_pthread_destroy end");
}

void *pthread_routine_wrapper(void *__arg) {

    auto *specific = (char *) malloc(sizeof(char));
    *specific = 'P';

    if (!m_key) {
        pthread_key_create(&m_key, on_pthread_destroy);
    }

    pthread_setspecific(m_key, specific);

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

    on_pthread_create(*__pthread_ptr);

    return ret;
}

DEFINE_HOOK_FUN(int, pthread_setname_np, pthread_t __pthread, const char *__name) {
    CALL_ORIGIN_FUNC_RET(int, ret, pthread_setname_np, __pthread, __name);
    on_pthread_setname(__pthread, __name);
    return ret;
}
