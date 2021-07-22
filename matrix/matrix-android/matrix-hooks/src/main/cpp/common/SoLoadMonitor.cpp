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

namespace matrix {
    static std::atomic_bool sInstalled(false);
    static std::vector<so_load_callback_t> sSoLoadCallbacks;
    static std::mutex sDlOpenBlockerMutex;
    static std::condition_variable sDlOpenBlocker;
    static bool sDlOpenPaused = false;

    static void NotifySoLoad(const char* pathname) {
        if (UNLIKELY(pathname == nullptr)) {
            // pathname == nullptr indicates dlopen was called with RTLD_DEFAULT, which doesn't load any new libraries.
            // So we can skip the rest logic below in such situation.
            return;
        }

        PauseLoadSo();

        xhook_refresh(0);

        wechat_backtrace::notify_maps_changed();

        for (auto cb : sSoLoadCallbacks) {
            cb(pathname);
        }

        ResumeLoadSo();
    }

    static void* (*orig__loader_dlopen)(const char* filename, int flags, const void* caller_addr) = nullptr;
    static void* __loader_dlopen_handler(const char* filename, int flags, const void* caller_addr) {
        {
            std::unique_lock lock(sDlOpenBlockerMutex);
            if (sDlOpenPaused) {
                sDlOpenBlocker.wait(lock, []() { return !sDlOpenPaused; });
            }
        }
        void* hLib = orig__loader_dlopen(filename, flags, caller_addr);
        if (hLib != nullptr && (flags & RTLD_NOLOAD) == 0) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    static void* (*orig__loader_android_dlopen_ext)(const char* filename,
            int flag, const void* extinfo, const void* caller_addr) = nullptr;
    static void* __loader_android_dlopen_ext_handler(const char* filename,
            int flag, const void* extinfo, const void* caller_addr) {
        {
            std::unique_lock lock(sDlOpenBlockerMutex);
            if (sDlOpenPaused) {
                sDlOpenBlocker.wait(lock, []() { return !sDlOpenPaused; });
            }
        }
        void* hLib = orig__loader_android_dlopen_ext(filename, flag, extinfo, caller_addr);
        if (hLib != nullptr && (flag & RTLD_NOLOAD) == 0) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    static void* (*orig_dlopen)(const char* filename, int flags) = nullptr;
    static void* dlopen_handler(const char* filename, int flags) {
        {
            std::unique_lock lock(sDlOpenBlockerMutex);
            if (sDlOpenPaused) {
                sDlOpenBlocker.wait(lock, []() { return !sDlOpenPaused; });
            }
        }
        void* hLib = orig_dlopen(filename, flags);
        if (hLib != nullptr && (flags & RTLD_NOLOAD) == 0) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    static void* (*orig_android_dlopen_ext)(const char* filename, int flag, const void* extinfo) = nullptr;
    static void* android_dlopen_ext_handler(const char* filename, int flag, const void* extinfo) {
        {
            std::unique_lock lock(sDlOpenBlockerMutex);
            if (sDlOpenPaused) {
                sDlOpenBlocker.wait(lock, []() { return !sDlOpenPaused; });
            }
        }
        void* hLib = orig_android_dlopen_ext(filename, flag, extinfo);
        if (hLib != nullptr && (flag & RTLD_NOLOAD) == 0) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    #ifdef __LP64__
    #define LINKER_NAME "linker64"
    #define LINKER_NAME_PATTERN ".*/linker64$"
    #else
    #define LINKER_NAME "linker"
    #define LINKER_NAME_PATTERN ".*/linker$"
    #endif

    bool InstallSoLoadMonitor() {
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
                LOGE(LOG_TAG, "Fail to find original __loader_dlopen.");
                return false;
            }
            if (UNLIKELY(xhook_register(".*\\.so$", "__loader_dlopen",
                    reinterpret_cast<void*>(__loader_dlopen_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register __loader_dlopen hook handler.");
                return false;
            }

            orig__loader_android_dlopen_ext = reinterpret_cast<decltype(orig__loader_android_dlopen_ext)>(
                    matrix::SemiDlSym(hLinker, "__dl___loader_android_dlopen_ext"));
            if (UNLIKELY(orig__loader_android_dlopen_ext == nullptr)) {
                LOGE(LOG_TAG, "Fail to find original __loader_android_dlopen_ext.");
                return false;
            }
            if (UNLIKELY(xhook_register(".*\\.so$", "__loader_android_dlopen_ext",
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
            if (UNLIKELY(xhook_register(".*\\.so$", "dlopen",
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
            if (UNLIKELY(xhook_register(".*\\.so$", "android_dlopen_ext",
                    reinterpret_cast<void*>(android_dlopen_ext_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register android_dlopen_ext hook handler.");
                return false;
            }
        }
        xhook_ignore(LINKER_NAME_PATTERN, nullptr);
        xhook_ignore(".*/libpatrons\\.so$", nullptr);
        xhook_ignore(".*/libwebviewchromium_loader\\.so$", nullptr);
        sInstalled = true;
        return true;
    }

    static void EnsureInstalled() {
        if (UNLIKELY(!sInstalled)) {
            __android_log_assert(nullptr, LOG_TAG, "Please call Install first and confirm it returns true.");
        }
    }

    void AddOnSoLoadCallback(so_load_callback_t cb) {
        EnsureInstalled();
        PauseLoadSo();
        sSoLoadCallbacks.push_back(cb);
        ResumeLoadSo();
    }

    void PauseLoadSo() {
        EnsureInstalled();
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