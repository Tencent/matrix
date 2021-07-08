//
// Created by YinSheng Tang on 2021/7/8.
//

#include <vector>
#include <mutex>
#include <backtrace/Backtrace.h>
#include <xhook.h>
#include <xhook_ext.h>
#include <android/log.h>
#include "ScopedCleaner.h"
#include "Log.h"
#include "HookCommon.h"
#include "SoLoadMonitor.h"
#include "SemiDlfcn.h"

#define LOG_TAG "Matrix.SoLoadMonitor"

namespace matrix {
    static std::atomic_bool sInstalled(false);
    static std::vector<so_load_callback_t> sSoLoadCallbacks;
    static std::recursive_mutex sDlOpenMutex;

    static void NotifySoLoad(const char* pathname) {
        wechat_backtrace::notify_maps_changed();

        for (auto cb : sSoLoadCallbacks) {
            cb(pathname);
        }
    }

    static void* (*do_dlopen_ge_N)(const char* name, int flags, const void* extinfo, const void* caller_addr) = nullptr;
    static void* (*do_dlopen_lt_N)(const char* name, int flags, const void* extinfo) = nullptr;

    static void* __loader_dlopen_handler(const char* filename, int flags, const void* caller_addr) {
        std::lock_guard dlopenLock(sDlOpenMutex);
        void *hLib = do_dlopen_ge_N(filename, flags, nullptr, caller_addr);
        if (hLib != nullptr) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    static void* __loader_android_dlopen_ext_handler(const char* filename,
                                                     int flag, const void* extinfo, const void* caller_addr) {
        std::lock_guard dlopenLock(sDlOpenMutex);
        void *hLib = do_dlopen_ge_N(filename, flag, extinfo, caller_addr);
        if (hLib != nullptr) {
            NotifySoLoad(filename);
        }
        return hLib;
    }

    static void* dlopen_handler(const char* filename, int flags) {
        int sdkVer = android_get_device_api_level();
        {
            std::lock_guard dlopenLock(sDlOpenMutex);
            void *hLib = nullptr;
            if (sdkVer >= 24) {
                void* callerAddr = __builtin_return_address(0);
                hLib = do_dlopen_ge_N(filename, flags, nullptr, callerAddr);
            } else {
                hLib = do_dlopen_lt_N(filename, flags, nullptr);
            }
            if (hLib != nullptr) {
                NotifySoLoad(filename);
            }
            return hLib;
        }
    }

    static void* android_dlopen_ext_handler(const char* filename, int flag, const void* extinfo) {
        int sdkVer = android_get_device_api_level();
        {
            std::lock_guard dlopenLock(sDlOpenMutex);
            void *hLib = nullptr;
            if (sdkVer >= 24) {
                void* callerAddr = __builtin_return_address(0);
                hLib = do_dlopen_ge_N(filename, flag, extinfo, callerAddr);
            } else {
                hLib = do_dlopen_lt_N(filename, flag, extinfo);
            }
            if (hLib != nullptr) {
                NotifySoLoad(filename);
            }
            return hLib;
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
        if (sInstalled) {
            return true;
        }

        int sdkVer = android_get_device_api_level();

        {
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

            if (sdkVer >= 24) {
                do_dlopen_ge_N = reinterpret_cast<decltype(do_dlopen_ge_N)>(
                        matrix::SemiDlSym(hLinker, "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv"));
                if (UNLIKELY(do_dlopen_ge_N == nullptr)) {
                    do_dlopen_ge_N = reinterpret_cast<decltype(do_dlopen_ge_N)>(
                            matrix::SemiDlSym(hLinker, "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"));
                }
                if (UNLIKELY(do_dlopen_ge_N == nullptr)) {
                    LOGE(LOG_TAG, "Fail to fetch address of do_dlopen.");
                    return false;
                }
            } else {
                do_dlopen_lt_N = reinterpret_cast<decltype(do_dlopen_lt_N)>(
                        matrix::SemiDlSym(hLinker, "__dl__Z9do_dlopenPKciPK17android_dlextinfo"));
                if (UNLIKELY(do_dlopen_lt_N == nullptr)) {
                    LOGE(LOG_TAG, "Fail to fetch address of do_dlopen.");
                    return false;
                }
            }
        }

        if (sdkVer >= 26 /* O */) {
            if (UNLIKELY(xhook_register(".*\\.so$", "__loader_dlopen",
                    reinterpret_cast<void*>(__loader_dlopen_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register __loader_dlopen hook handler.");
                return false;
            }
            if (UNLIKELY(xhook_register(".*\\.so$", "__loader_android_dlopen_ext",
                    reinterpret_cast<void*>(__loader_android_dlopen_ext_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register __loader_android_dlopen_ext hook handler.");
                return false;
            }
        } else {
            if (UNLIKELY(xhook_register(".*\\.so$", "dlopen",
                    reinterpret_cast<void*>(dlopen_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register dlopen hook handler.");
                return false;
            }
            if (UNLIKELY(xhook_register(".*\\.so$", "android_dlopen_ext",
                    reinterpret_cast<void*>(android_dlopen_ext_handler), nullptr) != 0)) {
                LOGE(LOG_TAG, "Fail to register android_dlopen_ext hook handler.");
                return false;
            }
        }
        xhook_ignore(LINKER_NAME_PATTERN, nullptr);
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
        sDlOpenMutex.lock();
    }

    void ResumeLoadSo() {
        EnsureInstalled();
        sDlOpenMutex.unlock();
    }
}