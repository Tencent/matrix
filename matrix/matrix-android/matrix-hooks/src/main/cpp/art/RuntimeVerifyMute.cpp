//
// Created by tomystang on 2022/11/17.
//

#include <mutex>
#include <jni.h>
#include <sys/system_properties.h>
#include <android/api-level.h>
#include <semi_dlfcn.h>
#include <Log.h>
#include <common/ScopedCleaner.h>
#include "RuntimeVerifyMute.h"

#define LOG_TAG "Matrix.RuntimeVerifyMute"

static bool sInitialized = false;
static std::mutex sInitLock;

bool matrix::art_misc::Install(JNIEnv* env) {
    if (sInitialized) {
        LOGI(LOG_TAG, "[!] Already installed.");
        return true;
    }
    std::lock_guard lock(sInitLock);
    if (sInitialized) {
        LOGI(LOG_TAG, "[!] Already installed.");
        return true;
    }

    int sdk_ver = android_get_device_api_level();
    if (sdk_ver == -1) {
        LOGE(LOG_TAG, "[-] Fail to get sdk version.");
        return false;
    }
    if (sdk_ver < __ANDROID_API_Q__) {
        LOGE(LOG_TAG, "[-] SDK version is lower than Q.");
        return false;
    }

    void* h_libart = semi_dlopen("libart.so");
    if (h_libart == nullptr) {
        LOGE(LOG_TAG, "[-] Fail to open libart.so.");
        return false;
    }
    auto h_libart_cleaner = MakeScopedCleaner([&h_libart]() {
        if (h_libart != nullptr) {
            semi_dlclose(h_libart);
        }
    });

    void** art_runtime_instance_ptr =
            reinterpret_cast<void**>(semi_dlsym(h_libart, "_ZN3art7Runtime9instance_E"));
    if (art_runtime_instance_ptr == nullptr) {
        LOGE(LOG_TAG, "[-] Fail to find Runtime::instance_.");
        return false;
    }
    void (*art_runtime_disable_verifier_fn)(void*) =
            reinterpret_cast<void (*)(void*)>(semi_dlsym(h_libart, "_ZN3art7Runtime15DisableVerifierEv"));
    if (art_runtime_disable_verifier_fn == nullptr) {
        LOGE(LOG_TAG, "[-] Fail to find Runtime::DisableVerifier().");
        return false;
    }

    art_runtime_disable_verifier_fn(*art_runtime_instance_ptr);

    LOGI(LOG_TAG, "[+] Runtime::DisableVerifier() was invoked.");

    return true;
}