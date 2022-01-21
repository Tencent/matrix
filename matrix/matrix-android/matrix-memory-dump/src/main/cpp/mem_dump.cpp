#include <jni.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <cstdlib>
#include <semi_dlfcn.h>

#include "bionic/tls.h"
#include "internal/log.h"
#include "runtime/collector_type.h"
#include "runtime/gc_cause.h"
#include "dlfcn/self_dlfcn.h"

#define LOG_TAG "Matrix.MemoryDump"

#define READ 0
#define WRITE 1

struct fork_pair {
    int pid;
    int fd;
};

using namespace art::gc;

namespace mirror {

    static void *suspend_all_ptr_ = nullptr;

    static void *resume_all_ptr_ = nullptr;

    class Thread {
    };

    class ScopedSuspend {
    };

    static void (*sgc_constructor)(void *, Thread *, GcCause, CollectorType) = nullptr;

    static void (*sgc_destructor)(void *) = nullptr;

    class ScopedGCCriticalSection {
    private:
        uint64_t buf[8] = {0};
    public:
        ScopedGCCriticalSection(Thread *thread, GcCause cause, CollectorType collectorType) {
            if (sgc_constructor != nullptr) {
                sgc_constructor(this, thread, cause, collectorType);
            }
        }

        ~ScopedGCCriticalSection() {
            if (sgc_destructor != nullptr) {
                sgc_destructor(this);
            }
        }
    };

    static void (*exclusive_lock)(void *, Thread *) = nullptr;

    static void (*exclusive_unlock)(void *, Thread *) = nullptr;

    class ReadWriteMutex {
    public:
        void ExclusiveLock(Thread *self) {
            if (exclusive_lock != nullptr) {
                reinterpret_cast<void (*)(void *, Thread *)>(exclusive_lock)(this, self);
            }
        }

        void ExclusiveUnlock(Thread *self) {
            if (exclusive_unlock != nullptr) {
                reinterpret_cast<void (*)(void *, Thread *)>(exclusive_unlock)(this, self);
            }
        }
    };
}

static int android_version_;

static mirror::ScopedSuspend suspend_;

static mirror::ReadWriteMutex *mutator_lock_ = nullptr;

static void (*dump_heap_)(const char *, int, bool) = nullptr;

static void suspend_runtime(mirror::Thread *thread) {
    if (android_version_ > __ANDROID_API_Q__) {
        mirror::ScopedGCCriticalSection sgc(thread, kGcCauseHprof, kCollectorTypeHprof);

        if (mirror::suspend_all_ptr_ != nullptr) {
            reinterpret_cast<void (*)(mirror::ScopedSuspend *, const char *, bool)>
            (mirror::suspend_all_ptr_)(&suspend_, "matrix_dump_hprof", true);
        } else {
            _error_log(LOG_TAG,
                       "Cannot suspend runtime because suspend function symbol cannot be found.");
        }

        mutator_lock_->ExclusiveUnlock(thread);
    } else {
        if (mirror::suspend_all_ptr_ != nullptr) {
            reinterpret_cast<void (*)()>(mirror::suspend_all_ptr_)();
        } else {
            _error_log(LOG_TAG,
                       "Cannot suspend runtime because suspend function symbol cannot be found.");
        }
    }
}

static void resume_runtime(mirror::Thread *thread) {
    if (android_version_ > __ANDROID_API_Q__) {
        mutator_lock_->ExclusiveLock(thread);

        if (mirror::resume_all_ptr_ != nullptr) {
            reinterpret_cast<void (*)(mirror::ScopedSuspend *)>(mirror::resume_all_ptr_)(&suspend_);
        } else {
            _error_log(LOG_TAG,
                       "Cannot suspend runtime because suspend function symbol cannot be found.");
        }
    } else {
        if (mirror::resume_all_ptr_ != nullptr) {
            reinterpret_cast<void (*)()>(mirror::resume_all_ptr_)();
        } else {
            _error_log(LOG_TAG,
                       "Cannot resume runtime because resume function symbol cannot be found.");
        }
    }
}

static bool initialize_functions(
        void *(*_dl_open)(const char *),
        void *(*_dl_sym)(void *, const char *),
        void (*_dl_close)(void *),
        void (*_dl_clean)(void *),
        const char *impl_tag
) {
    auto *art_lib = _dl_open("libart.so");

    if (art_lib == nullptr) {
        _error_log(LOG_TAG, "Cannot dynamic open library libart.so with %s.", impl_tag);
        return false;
    }

    auto *lock_sym = reinterpret_cast<mirror::ReadWriteMutex **>(
            _dl_sym(art_lib, "_ZN3art5Locks13mutator_lock_E"));
    if (lock_sym == nullptr) {
        _error_log(LOG_TAG, "Cannot find symbol art::Locks::mutator_lock_.");
        goto on_error;
    } else {
        mutator_lock_ = *lock_sym;
    }

#define _load_symbol_unsafe(ptr, type, sym, err)         \
    ptr = reinterpret_cast<type>(_dl_sym(art_lib, sym)); \
    if (ptr == nullptr) {                                \
        _info_log(LOG_TAG, err);                         \
    }

#define _load_symbol(ptr, type, sym, err)                \
    ptr = reinterpret_cast<type>(_dl_sym(art_lib, sym)); \
    if (ptr == nullptr) {                                \
        _error_log(LOG_TAG, err);                        \
        goto on_error;                                   \
    }

    _load_symbol_unsafe(dump_heap_,
                        void(*)(const char *, int, bool ),
                        "_ZN3art5hprof8DumpHeapEPKcib",
                        "Cannot find symbol art::hprof::DumpHeap")

    if (android_version_ > __ANDROID_API_Q__) {
        _load_symbol(mirror::suspend_all_ptr_,
                     void*,
                     "_ZN3art16ScopedSuspendAllC1EPKcb",
                     "Cannot find symbol art::ScopedSuspendAll().")
        _load_symbol(mirror::resume_all_ptr_,
                     void*,
                     "_ZN3art16ScopedSuspendAllD1Ev",
                     "Cannot find symbol art::~ScopedSuspendAll().")

        _load_symbol(mirror::sgc_constructor,
                     void(*)(void * , mirror::Thread *, GcCause, CollectorType),
                     "_ZN3art2gc23ScopedGCCriticalSectionC1EPNS_6ThreadENS0_7GcCauseENS0_13CollectorTypeE",
                     "Cannot find symbol art::gc::ScopedGCCriticalSection().")
        _load_symbol(mirror::sgc_destructor,
                     void(*)(void * ),
                     "_ZN3art2gc23ScopedGCCriticalSectionD1Ev",
                     "Cannot find symbol art::gc::~ScopedGCCriticalSection().")

        _load_symbol(mirror::exclusive_lock,
                     void(*)(void * , mirror::Thread *),
                     "_ZN3art17ReaderWriterMutex13ExclusiveLockEPNS_6ThreadE",
                     "Cannot find symbol art::ReaderWriterMutex::ExclusiveLock().")
        _load_symbol(mirror::exclusive_unlock,
                     void(*)(void * , mirror::Thread *),
                     "_ZN3art17ReaderWriterMutex15ExclusiveUnlockEPNS_6ThreadE",
                     "Cannot find symbol art::ReaderWriterMutex::ExclusiveUnlock().")
    } else {
        _load_symbol(mirror::suspend_all_ptr_,
                     void*,
                     "_ZN3art3Dbg9SuspendVMEv",
                     "Cannot find symbol art::Dbg::SuspendVM.")

        _load_symbol(mirror::resume_all_ptr_,
                     void*,
                     "_ZN3art3Dbg8ResumeVMEv",
                     "Cannot find symbol art::Dbg::ResumeVM.")
    }

    _dl_clean(art_lib);
    return true;

    on_error:
    _dl_close(art_lib);
    return false;
}

static bool initialize_functions_with_self() {
    return initialize_functions(self_dlopen, self_dlsym, self_dlclose, self_clean, "self_dlfcn");
}

static bool initialize_functions_with_semi() {
    return initialize_functions(
            semi_dlopen,
            reinterpret_cast<void *(*)(void *, const char *)>(semi_dlsym),
            semi_dlclose,
            semi_dlclose,
            "semi_dlfcn"
    );
}

static void fork_process_crash_handler(int signal_num) {
    // Do nothing, just tell parent process to handle the error.
    exit(-3);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_initializeNative(JNIEnv *, jclass) {

    {
        char api_level[5];
        if (__system_property_get("ro.build.version.sdk", api_level) < 1) {
            _error_log(LOG_TAG, "Unable to get system property ro.build.version.sdk.");
        }
        android_version_ = static_cast<int>(strtol(api_level, nullptr, 10));
    }

    // Set Android API version first to decide whether to use native "dlfcn" implementation or not.
    self_dlfcn_mode(android_version_);
    bool ret = initialize_functions_with_self();
    if (!ret) {
        ret = initialize_functions_with_semi();
    }
    return ret;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_dumpHprof(JNIEnv *, jclass, jint fd) {
    if (dump_heap_ != nullptr) {
        dump_heap_("[fd]", fd, false);
        return 0;
    } else {
        _error_log(LOG_TAG, "Failed to load art::hprof::DumpHeap().");
        return -1;
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_fork(JNIEnv *, jclass, jlong child_timeout) {
    auto *thread = reinterpret_cast<mirror::Thread *>(__get_tls()[TLS_SLOT_ART_THREAD_SELF]);
    suspend_runtime(thread);
    int pid = fork();
    if (pid == 0) {
        signal(SIGSEGV, fork_process_crash_handler);
        alarm(child_timeout);
        prctl(PR_SET_NAME, "matrix_dump_process");
    } else {
        resume_runtime(thread);
    }
    return pid;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_forkPipe(JNIEnv *, jclass, jlong child_timeout) {
    int fd[2];
    if (pipe(fd)) {
        return -2;
    }

    auto *ret = reinterpret_cast<fork_pair *>(malloc(sizeof(fork_pair)));
    if (ret == nullptr) {
        close(fd[READ]);
        close(fd[WRITE]);
        return -2;
    }

    auto *thread = reinterpret_cast<mirror::Thread *>(__get_tls()[TLS_SLOT_ART_THREAD_SELF]);
    suspend_runtime(thread);

    int pid = fork();
    ret->pid = pid;
    if (pid == 0) {
        close(fd[READ]);
        ret->fd = fd[WRITE];
        signal(SIGSEGV, fork_process_crash_handler);
        alarm(child_timeout);
        prctl(PR_SET_NAME, "matrix_dump_process");
        return reinterpret_cast<jlong>(ret);
    } else {
        resume_runtime(thread);
        close(fd[WRITE]);
        if (pid == -1) {
            close(fd[READ]);
            free(ret);
            return -1;
        } else {
            ret->fd = fd[READ];
            return reinterpret_cast<jlong>(ret);
        }
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_wait(JNIEnv *, jclass, jint pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        _error_log(LOG_TAG, "Failed to invoke waitpid().");
        return -2;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else {
        _error_log(LOG_TAG, "Fork process exited unexpectedly.");
        return -2;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_exit(JNIEnv *, jclass, jint code) {
    _exit(code);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_pidFromForkPair(JNIEnv *, jclass, jlong pointer) {
    return reinterpret_cast<fork_pair *>(pointer)->pid;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_fdFromForkPair(JNIEnv *, jclass, jlong pointer) {
    return reinterpret_cast<fork_pair *>(pointer)->fd;
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_matrix_memorydump_MemoryDumpKt_free(JNIEnv *, jclass, jlong pointer) {
    free(reinterpret_cast<void *>(pointer));
}