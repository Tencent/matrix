#include "symbol.h"

#include <sys/system_properties.h>

#include "bionic/tls.h"
#include "bionic/tls_defines.h"
#include "dlsym/dlsym.h"
#include "../log.h"
#include "runtime/collector_type.h"
#include "runtime/gc_cause.h"

#define LOG_TAG "Matrix.MemoryUtil.Symbol"

static int android_version_;

using namespace art::gc;

namespace mirror {
    class ScopedSuspend {
    };

    static void (*sgc_constructor)(void *, Thread *, GcCause, CollectorType) = nullptr;

    static void (*sgc_destructor)(void *) = nullptr;

    class ScopedGCCriticalSection {
    private:
        uint64_t buf[8] = {0};
    public:
        ScopedGCCriticalSection(Thread *thread, GcCause cause, CollectorType collectorType) {
            sgc_constructor(this, thread, cause, collectorType);
        }

        ~ScopedGCCriticalSection() {
            sgc_destructor(this);
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

mirror::Thread *current_thread() {
    return reinterpret_cast<mirror::Thread *>(__get_tls()[TLS_SLOT_ART_THREAD_SELF]);
}

static mirror::ScopedSuspend suspend_;

static mirror::ReadWriteMutex *mutator_lock_ = nullptr;

static void *suspend_all_ptr_ = nullptr;

void suspend_runtime(mirror::Thread *thread) {
    if (android_version_ > __ANDROID_API_Q__) {
        mirror::ScopedGCCriticalSection sgc(thread, kGcCauseHprof, kCollectorTypeHprof);

        if (suspend_all_ptr_ != nullptr) {
            reinterpret_cast<void (*)(mirror::ScopedSuspend *, const char *, bool)>
            (suspend_all_ptr_)(&suspend_, "matrix_dump_hprof", true);
        } else {
            _error_log(LOG_TAG,
                       "Cannot suspend symbol.runtime because suspend function symbol cannot be found.");
        }

        mutator_lock_->ExclusiveUnlock(thread);
    } else {
        reinterpret_cast<void (*)()>(suspend_all_ptr_)();
    }
}

static void *resume_all_ptr_ = nullptr;

void resume_runtime(mirror::Thread *thread) {
    if (android_version_ > __ANDROID_API_Q__) {
        mutator_lock_->ExclusiveLock(thread);
        reinterpret_cast<void (*)(mirror::ScopedSuspend *)>(resume_all_ptr_)(&suspend_);
    } else {
        reinterpret_cast<void (*)()>(resume_all_ptr_)();
    }
}

bool initialize_symbols() {
    android_version_ = android_get_device_api_level();
    if (android_version_ <= 0) return false;
    ds_mode(android_version_);

    auto *art_lib = ds_open("libart.so");

    if (art_lib == nullptr) {
        _error_log(LOG_TAG, "Cannot dynamic open library libart.so.");
        return false;
    }

#define _load_symbol(ptr, type, sym, err)                \
    ptr = reinterpret_cast<type>(ds_find(art_lib, sym)); \
    if (ptr == nullptr) {                                \
        _error_log(LOG_TAG, err);                        \
        goto on_error;                                   \
    }

    if (android_version_ > __ANDROID_API_Q__) {
        _load_symbol(mirror::sgc_constructor,
                     void(*)(void * , mirror::Thread *, art::gc::GcCause, art::gc::CollectorType),
                     "_ZN3art2gc23ScopedGCCriticalSectionC1EPNS_6ThreadENS0_7GcCauseENS0_13CollectorTypeE",
                     "Cannot find symbol art::gc::ScopedGCCriticalSection().")
        _load_symbol(mirror::sgc_destructor,
                     void(*)(void * ),
                     "_ZN3art2gc23ScopedGCCriticalSectionD1Ev",
                     "Cannot find symbol art::gc::~ScopedGCCriticalSection().")
    }

    if (android_version_ > __ANDROID_API_Q__) {
        mirror::ReadWriteMutex **lock_sym;
        _load_symbol(lock_sym,
                     mirror::ReadWriteMutex **,
                     "_ZN3art5Locks13mutator_lock_E",
                     "Cannot find symbol art::Locks::mutator_lock_.")
        mutator_lock_ = *lock_sym;

        _load_symbol(mirror::exclusive_lock,
                     void(*)(void * , mirror::Thread *),
                     "_ZN3art17ReaderWriterMutex13ExclusiveLockEPNS_6ThreadE",
                     "Cannot find symbol art::ReaderWriterMutex::ExclusiveLock().")
        _load_symbol(mirror::exclusive_unlock,
                     void(*)(void * , mirror::Thread *),
                     "_ZN3art17ReaderWriterMutex15ExclusiveUnlockEPNS_6ThreadE",
                     "Cannot find symbol art::ReaderWriterMutex::ExclusiveUnlock().")
    }

    if (android_version_ > __ANDROID_API_Q__) {
        _load_symbol(suspend_all_ptr_,
                     void*,
                     "_ZN3art16ScopedSuspendAllC1EPKcb",
                     "Cannot find symbol art::ScopedSuspendAll().")
        _load_symbol(resume_all_ptr_,
                     void*,
                     "_ZN3art16ScopedSuspendAllD1Ev",
                     "Cannot find symbol art::~ScopedSuspendAll().")
    } else {
        _load_symbol(suspend_all_ptr_,
                     void*,
                     "_ZN3art3Dbg9SuspendVMEv",
                     "Cannot find symbol art::Dbg::SuspendVM().")
        _load_symbol(resume_all_ptr_,
                     void*,
                     "_ZN3art3Dbg8ResumeVMEv",
                     "Cannot find symbol art::Dbg::ResumeVM().")
    }

    ds_clean(art_lib);
    return true;

    on_error:
    ds_close(art_lib);
    return false;
}
