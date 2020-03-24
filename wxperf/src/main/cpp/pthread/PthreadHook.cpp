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

#define THREAD_NAME_LEN 16

extern "C" typedef struct {
    pthread_t                           pthread; // key ?
    pid_t                               tid;
    char                                *thread_name;
    char                                *parent_name;
    char                                *java_stacktrace;
    std::vector<unwindstack::FrameData> *native_stacktrace;
} pthread_meta_t;

struct regex_wrapper {
    const char * regex_str;
    regex_t regex;

    regex_wrapper(const char *regexStr, const regex_t &regex) : regex_str(regexStr), regex(regex) {}

    friend bool operator< (const regex_wrapper &left, const regex_wrapper &right) {
        return static_cast<bool>(strcmp(left.regex_str, right.regex_str));
    }
};

//extern "C" typedef struct {
//    regex_t thread_name_regex;
//    regex_t parent_thread_name_regex;
//} hook_info_t;

static pthread_mutex_t                               m_thread_meta_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::unordered_map<pthread_t, pthread_meta_t> m_thread_metas;

static std::set<regex_wrapper> m_hook_thread_name_regex;
//static std::set<regex_t> m_hook_parent_thread_name_regex;

void add_hook_thread_name(const char *__regex_str) {
//    std::regex regex(__regex_str);
    regex_t regex;
    if (0 != regcomp(&regex, __regex_str, REG_NOSUB)) {
        LOGE("PthreadHook", "regex compiled error: %s", __regex_str);
        return;
    }
    size_t len = strlen(__regex_str);
    char *p_regex_str = static_cast<char *>(malloc(sizeof(char) * len));
    strcpy(p_regex_str, __regex_str);
    regex_wrapper w_regex(p_regex_str, regex);
    m_hook_thread_name_regex.insert(w_regex);
    LOGD("Yves-debug", "parent name regex: %s", __regex_str);
}

static int read_thread_name(pthread_t __pthread, char *__buf, size_t __n) {
    if (!__buf) {
        return -1;
    }

    char proc_path[128];

    sprintf(proc_path, "/proc/self/task/%d/stat", pthread_gettid_np(__pthread));

    FILE *file = fopen(proc_path, "r");

    if (!file) {
        LOGD("Yves-debug", "file not found: %s", proc_path);
        return -1;
    }

    fscanf(file, "%*d (%[^)]", __buf);

    fclose(file);

    return 0;
}

static inline int wrap_pthread_getname_np(pthread_t __pthread, char *__buf, size_t __n) {
#if __ANDROID_API__ >= 26
    return pthread_getname_np(__pthread, __buf, __n);
#else
    return read_thread_name(__pthread, __buf, __n);
#endif
}

static void unwind_pthread_stacktrace(pthread_meta_t &__meta) {
    for (const auto& w: m_hook_thread_name_regex) {
        if (0 == regexec(&w.regex, __meta.thread_name, 0, NULL, 0)) {
            LOGD("Yves-debug", "%s matches regex %s", __meta.thread_name, w.regex_str);
            // do unwind
            auto native_stacktrace = new std::vector<unwindstack::FrameData>;
            unwindstack::do_unwind(*native_stacktrace);

            if (!native_stacktrace->empty()) {
                __meta.native_stacktrace = native_stacktrace;
            } else {
                delete native_stacktrace;
            }

            auto java_stacktrace = static_cast<char *>(malloc(sizeof(char) * 1024));
//            if (get_java_stacktrace(java_stacktrace)) {
//                __meta.java_stacktrace = java_stacktrace;
//            } else {
//                free(java_stacktrace);
//            }

            strcpy(java_stacktrace, "    stub java stack");
            __meta.java_stacktrace = java_stacktrace;

            break;
        }
    }
}


static void on_pthread_create(const pthread_t *__pthread_ptr) {
    LOGD("Yves-debug", "on_pthread_create");
    pthread_mutex_lock(&m_thread_meta_mutex);

    pthread_t pthread = *__pthread_ptr;
    if (m_thread_metas.count(pthread)) {
        pthread_mutex_unlock(&m_thread_meta_mutex);
        return;
    }

    pthread_meta_t &meta = m_thread_metas[pthread];


    pid_t tid = pthread_gettid_np(pthread);
    LOGD("Yves-debug", "pthread = %ld, tid = %d", pthread, tid);

    char *parent_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));

    if (wrap_pthread_getname_np(*__pthread_ptr, parent_name, THREAD_NAME_LEN) == 0) {
        meta.thread_name = meta.parent_name = parent_name; // 子线程会继承父线程名
    } else {
        free(parent_name);
        meta.thread_name = meta.parent_name = const_cast<char *>("null");
    }

    LOGD("Yves-debug", "pthread = %ld, parent name: %s, thread name: %s", pthread, meta.parent_name, meta.thread_name);
    unwind_pthread_stacktrace(meta);

    pthread_mutex_unlock(&m_thread_meta_mutex);
}

void on_pthread_setname(pthread_t __pthread, const char *__name) {
    if (NULL == __name) {
        LOGE("Yves-debug", "setting name null");
        return;
    }

    const size_t name_len = strlen(__name);
    LOGD("Yves-debug", "on_pthread_setname: %ld, %s, %zu", __pthread, __name, name_len);

    if (0 == name_len || name_len >= THREAD_NAME_LEN) {
        LOGE("Yves-debug", "pthread name is illegal, just ignore. len(%zu)", name_len);
        return;
    }

    pthread_mutex_lock(&m_thread_meta_mutex);

    if (!m_thread_metas.count(__pthread)) {
        LOGE("Yves-debug", "pthread hook lost");
        return;
    }

    pthread_meta_t &meta = m_thread_metas.at(__pthread);

    meta.thread_name = static_cast<char *>(malloc(sizeof(char) * THREAD_NAME_LEN));

    strncpy(meta.thread_name, __name, THREAD_NAME_LEN);

    // 如果有 set name, 并且子线程名与父线程名不一致, create 时父线程名没有匹配到正则
    LOGD("Yves-debug", "js:%s, pn:%s, tn:%s",meta.java_stacktrace, meta.parent_name, meta.thread_name);
    if ((!meta.native_stacktrace || meta.native_stacktrace->empty())
        && !meta.java_stacktrace
        && ((meta.parent_name == NULL && meta.thread_name == NULL) || 0 != strncmp(meta.parent_name, meta.thread_name, THREAD_NAME_LEN))) {
        LOGD("Yves-debug", "unwinding pthread");
        unwind_pthread_stacktrace(meta);
    }

    LOGD("Yves-debug", "--------------------------");
    pthread_mutex_unlock(&m_thread_meta_mutex);
}


void pthread_dump_impl(FILE *__log_file) {
    if (!__log_file) {
        LOGE("Yves-debug", "open file failed");
        return;
    }

    for (auto i: m_thread_metas) {
        auto meta = i.second;
        LOGD("Yves-debug", "pthread[%s], parent[%s], tid[%d]", meta.thread_name,
             meta.parent_name, meta.tid);
        fprintf(__log_file, "pthread[%s], parent[%s], tid[%d]\n", meta.thread_name,
                meta.parent_name, meta.tid);
        std::stringstream stack_builder;

        if (!meta.native_stacktrace) {
            continue;
        }

        for (auto p_frame = meta.native_stacktrace->begin();
             p_frame != meta.native_stacktrace->end(); ++p_frame) {
            Dl_info stack_info;
            dladdr((void *) p_frame->pc, &stack_info);

            std::string so_name = std::string(stack_info.dli_fname);

            int  status          = 0;
            char *demangled_name = abi::__cxa_demangle(stack_info.dli_sname, nullptr, 0,
                                                       &status);

            free(demangled_name);

            LOGD("Yves-debug", "    #pc %"
                    PRIxPTR
                    " %s (%s)", p_frame->rel_pc,
                 demangled_name ? demangled_name : "(null)", stack_info.dli_fname);
            fprintf(__log_file, "    #pc %" PRIxPTR " %s (%s)\n", p_frame->rel_pc,
                    demangled_name ? demangled_name : "(null)", stack_info.dli_fname);
        }

        LOGD("Yves-debug", "java stacktrace:\n%s", meta.java_stacktrace);
        fprintf(__log_file, "java stacktrace:\n%s\n", meta.java_stacktrace);
    }
}

void pthread_dump(const char *__path) {
    LOGD("Yves-debug",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump begin <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
    pthread_mutex_lock(&m_thread_meta_mutex);

    FILE *log_file = fopen(__path, "w+");
    LOGD("Yves-debug", "pthread dump path = %s", __path);

    pthread_dump_impl(log_file);

    fclose(log_file);

    pthread_mutex_unlock(&m_thread_meta_mutex);

    LOGD("Yves-debug",
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> pthread dump end <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
}

void pthread_hook_on_dlopen(const char *__file_name) {
    LOGD("Yves-debug", "pthread_hook_on_dlopen");
    unwindstack::update_maps();
}

DEFINE_HOOK_FUN(int, pthread_create, pthread_t *__pthread_ptr, pthread_attr_t const *__attr,
                void *(*__start_routine)(void *), void *__arg) {
    CALL_ORIGIN_FUNC_RET(int, ret, pthread_create, __pthread_ptr, __attr, __start_routine, __arg);
    on_pthread_create(__pthread_ptr);
    return ret;
}

DEFINE_HOOK_FUN(int, pthread_setname_np, pthread_t __pthread, const char *__name) {
    CALL_ORIGIN_FUNC_RET(int, ret, pthread_setname_np, __pthread, __name);
    on_pthread_setname(__pthread, __name);
    return ret;
}
