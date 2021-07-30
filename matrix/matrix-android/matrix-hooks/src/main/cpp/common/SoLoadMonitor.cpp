//
// Created by YinSheng Tang on 2021/7/8.
//

#include <vector>
#include <mutex>
#include <backtrace/Backtrace.h>
#include <xhook.h>
#include <xhook_ext.h>
#include <android/log.h>
#include <sys/resource.h>
#include <set>
#include "ScopedCleaner.h"
#include "Log.h"
#include "HookCommon.h"
#include "SoLoadMonitor.h"
#include "SemiDlfcn.h"

#define LOG_TAG "Matrix.SoLoadMonitor"

#define TLSVAR __thread __attribute__((tls_model("initial-exec")))

namespace matrix {
    static bool sInstalled = false;
    static std::mutex sInstalledMaskMutex;
    static std::mutex sSoLoadCallbacksMutex;
    static std::vector<so_load_callback_t> sSoLoadCallbacks;
    static std::mutex sDlOpenBlockerMutex;
    static std::condition_variable sDlOpenBlocker;
    static bool sDlOpenPaused = false;
    static std::recursive_mutex sDlOpenHandlerMutex;

    static void NotifySoLoad(const char* pathname) {
        if (UNLIKELY(pathname == nullptr)) {
            // pathname == nullptr indicates dlopen was called with RTLD_DEFAULT, which doesn't load any new libraries.
            // So we can skip the rest logic below in such situation.
            return;
        }

        wechat_backtrace::notify_maps_changed();

        size_t pathnameLen = strlen(pathname);
        if (pathnameLen >= 3) {
            if (strncmp(pathname + pathnameLen - 3, ".so", 3) == 0) {
                xhook_refresh(0);
                for (auto cb : sSoLoadCallbacks) {
                    cb(pathname);
                }
            }
        }
    }

    #define MARK_REENTERED(mask_tlsvar) \
        mask_tlsvar = true; \
        auto reenterCleaner = MakeScopedCleaner([]() { \
            mask_tlsvar = false; \
        })

    #define WAIT_ON_COND(mutex, cond_var, cond) \
        do { \
            std::unique_lock lock_##cond_var(mutex); \
            if ((cond)) { \
                (cond_var).wait(lock_##cond_var, []() { return !(cond); }); \
            } \
        } while (false)

    struct DlOpenArgs {
        const char* filename;
        int flags;
        const void* extinfo;
        const void* caller_addr;
    };

    static void* HandleDlOpenCommon(const std::function<void* (const DlOpenArgs*)> dlopen_fn, const DlOpenArgs* args) {
        std::lock_guard lock(sDlOpenHandlerMutex);
        void* hLib = dlopen_fn(args);
        if (hLib != nullptr && (args->flags & RTLD_NOLOAD) == 0) {
            NotifySoLoad(args->filename);
        }
        return hLib;
    }

    static TLSVAR bool __loader_dlopen_handler_reentered = false;
    static void* (*orig__loader_dlopen)(const char* filename, int flags, const void* caller_addr) = nullptr;
    static void* __loader_dlopen_handler(const char* filename, int flags, const void* caller_addr) {
        auto implFn = [](const DlOpenArgs* args) {
            return orig__loader_dlopen(args->filename, args->flags, args->caller_addr);
        };
        DlOpenArgs args = { .filename = filename, .flags = flags, .caller_addr = caller_addr };
        if (UNLIKELY(__loader_dlopen_handler_reentered)) {
            return HandleDlOpenCommon(implFn, &args);
        } else {
            MARK_REENTERED(__loader_dlopen_handler_reentered);
            WAIT_ON_COND(sDlOpenBlockerMutex, sDlOpenBlocker, sDlOpenPaused);
            return HandleDlOpenCommon(implFn, &args);
        }
    }

    static TLSVAR bool __loader_android_dlopen_ext_handler_reentered = false;
    static void* (*orig__loader_android_dlopen_ext)(const char* filename,
            int flags, const void* extinfo, const void* caller_addr) = nullptr;
    static void* __loader_android_dlopen_ext_handler(const char* filename,
                                                     int flags, const void* extinfo, const void* caller_addr) {
        auto implFn = [](const DlOpenArgs* args) {
            return orig__loader_android_dlopen_ext(args->filename, args->flags, args->extinfo, args->caller_addr);
        };
        DlOpenArgs args = { .filename = filename, .flags = flags, .extinfo = extinfo, .caller_addr = caller_addr };
        if (UNLIKELY(__loader_android_dlopen_ext_handler_reentered)) {
            return HandleDlOpenCommon(implFn, &args);
        } else {
            MARK_REENTERED(__loader_android_dlopen_ext_handler_reentered);
            WAIT_ON_COND(sDlOpenBlockerMutex, sDlOpenBlocker, sDlOpenPaused);
            return HandleDlOpenCommon(implFn, &args);
        }
    }

    static TLSVAR bool dlopen_handler_reentered = false;
    static void* (*orig_dlopen)(const char* filename, int flags) = nullptr;
    static void* dlopen_handler(const char* filename, int flags) {
        auto implFn = [](const DlOpenArgs* args) {
            return orig_dlopen(args->filename, args->flags);
        };
        DlOpenArgs args = { .filename = filename, .flags = flags };
        if (UNLIKELY(dlopen_handler_reentered)) {
            return HandleDlOpenCommon(implFn, &args);
        } else {
            MARK_REENTERED(dlopen_handler_reentered);
            WAIT_ON_COND(sDlOpenBlockerMutex, sDlOpenBlocker, sDlOpenPaused);
            return HandleDlOpenCommon(implFn, &args);
        }
    }

    static TLSVAR bool android_dlopen_ext_handler_reentered = false;
    static void* (*orig_android_dlopen_ext)(const char* filename, int flag, const void* extinfo) = nullptr;
    static void* android_dlopen_ext_handler(const char* filename, int flags, const void* extinfo) {
        auto implFn = [](const DlOpenArgs* args) {
            return orig_android_dlopen_ext(args->filename, args->flags, args->extinfo);
        };
        DlOpenArgs args = { .filename = filename, .flags = flags, .extinfo = extinfo };
        if (UNLIKELY(android_dlopen_ext_handler_reentered)) {
            return HandleDlOpenCommon(implFn, &args);
        } else {
            MARK_REENTERED(android_dlopen_ext_handler_reentered);
            WAIT_ON_COND(sDlOpenBlockerMutex, sDlOpenBlocker, sDlOpenPaused);
            return HandleDlOpenCommon(implFn, &args);
        }
    }

    #ifdef __LP64__
    #define LINKER_NAME "linker64"
    #define LINKER_NAME_PATTERN ".*/linker64$"
    #else
    #define LINKER_NAME "linker"
    #define LINKER_NAME_PATTERN ".*/linker$"
    #endif

    bool InstallSoLoadMonitor() {
        std::lock_guard lock(sInstalledMaskMutex);
        if (sInstalled) {
            return true;
        }

        int sdkVer = android_get_device_api_level();

        if (sdkVer == 24 || sdkVer == 25) {
            LOGE(LOG_TAG, "Does not support N and N_MR1 so far.");
            return false;
        }

        void *hLinker = matrix::SemiDlOpen(LINKER_NAME);
        if (hLinker == nullptr) {
            LOGE(LOG_TAG, "Fail to open %s.", LINKER_NAME);
            return false;
        }
        auto hLinkerCloser = MakeScopedCleaner([&hLinker]() {
            if (hLinker != nullptr) {
                matrix::SemiDlClose(hLinker);
            }
        });

        if (sdkVer >= 26 /* O */) {
            orig__loader_dlopen = reinterpret_cast<decltype(orig__loader_dlopen)>(
                    matrix::SemiDlSym(hLinker, "__dl___loader_dlopen"));
            if (UNLIKELY(orig__loader_dlopen == nullptr)) {
                orig__loader_dlopen = reinterpret_cast<decltype(orig__loader_dlopen)>(
                        matrix::SemiDlSym(hLinker, "__dl__Z8__dlopenPKciPKv"));
            }
            if (UNLIKELY(orig__loader_dlopen == nullptr)) {
                LOGE(LOG_TAG, "Fail to find original __loader_dlopen.");
                return false;
            }
            if (UNLIKELY(xhook_grouped_register(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*\\.so$", "__loader_dlopen",
                                                reinterpret_cast<void*>(__loader_dlopen_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register __loader_dlopen hook handler.");
                return false;
            }

            orig__loader_android_dlopen_ext = reinterpret_cast<decltype(orig__loader_android_dlopen_ext)>(
                    matrix::SemiDlSym(hLinker, "__dl___loader_android_dlopen_ext"));
            if (UNLIKELY(orig__loader_android_dlopen_ext == nullptr)) {
                orig__loader_android_dlopen_ext = reinterpret_cast<decltype(orig__loader_android_dlopen_ext)>(
                        matrix::SemiDlSym(hLinker, "__dl__Z20__android_dlopen_extPKciPK17android_dlextinfoPKv"));
            }
            if (UNLIKELY(orig__loader_android_dlopen_ext == nullptr)) {
                LOGE(LOG_TAG, "Fail to find original __loader_android_dlopen_ext.");
                return false;
            }
            if (UNLIKELY(xhook_grouped_register(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*\\.so$", "__loader_android_dlopen_ext",
                                                reinterpret_cast<void*>(__loader_android_dlopen_ext_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register __loader_android_dlopen_ext hook handler.");
                return false;
            }
        } else {
            orig_dlopen = reinterpret_cast<decltype(orig_dlopen)>(matrix::SemiDlSym(hLinker, "__dl_dlopen"));
            if (UNLIKELY(orig_dlopen == nullptr)) {
                LOGE(LOG_TAG, "Fail to find original dlopen.");
                return false;
            }
            if (UNLIKELY(xhook_grouped_register(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*\\.so$", "dlopen",
                                                reinterpret_cast<void*>(dlopen_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register dlopen hook handler.");
                return false;
            }

            orig_android_dlopen_ext = reinterpret_cast<decltype(orig_android_dlopen_ext)>(
                    matrix::SemiDlSym(hLinker, "__dl_android_dlopen_ext"));
            if (UNLIKELY(orig_android_dlopen_ext == nullptr)) {
                LOGE(LOG_TAG, "Fail to find original android_dlopen_ext.");
                return false;
            }
            if (UNLIKELY(xhook_grouped_register(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*\\.so$", "android_dlopen_ext",
                                                reinterpret_cast<void*>(android_dlopen_ext_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register android_dlopen_ext hook handler.");
                return false;
            }
        }
        xhook_grouped_ignore(HOOK_REQUEST_GROUPID_DLOPEN_MON, LINKER_NAME_PATTERN, nullptr);
        xhook_grouped_ignore(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*/libpatrons\\.so$", nullptr);
        xhook_grouped_ignore(HOOK_REQUEST_GROUPID_DLOPEN_MON, ".*/libwebviewchromium_loader\\.so$", nullptr);
        sInstalled = true;
        return true;
    }

    static void EnsureInstalled() {
        std::lock_guard lock(sInstalledMaskMutex);
        if (UNLIKELY(!sInstalled)) {
            __android_log_assert(nullptr, LOG_TAG, "Please call Install first and confirm it returns true.");
        }
    }

    void AddOnSoLoadCallback(so_load_callback_t cb) {
        EnsureInstalled();
        PauseLoadSo();
        {
            std::lock_guard lock(sSoLoadCallbacksMutex);
            sSoLoadCallbacks.push_back(cb);
        }
        ResumeLoadSo();
    }

    void PauseLoadSo() {
        EnsureInstalled();
        // If any dlopen procedure was on going, wait for it.
        std::lock_guard handlerLock(sDlOpenHandlerMutex);
        {
            std::lock_guard lock(sDlOpenBlockerMutex);
            sDlOpenPaused = true;
        }
    }

    void ResumeLoadSo() {
        EnsureInstalled();
        {
            std::lock_guard lock(sDlOpenBlockerMutex);
            sDlOpenPaused = false;
        }
        sDlOpenBlocker.notify_all();
    }
}