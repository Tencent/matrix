//
// Created by Yves on 2020-03-16.
//
#include <backtrace/QuickenUnwinder.h>
#include <backtrace/QuickenJNI.h>
#include <xh_errno.h>
#include <backtrace/common/PthreadExt.h>
#include <backtrace/Backtrace.h>
#include "JNICommon.h"
#include "Log.h"
#include "HookCommon.h"
#include "ScopedCleaner.h"
#include "xhook.h"

#define TAG "Matrix.JNICommon"

#ifdef __cplusplus
extern "C" {
#endif

JavaVM *m_java_vm;

jclass m_class_HookManager;
jmethodID m_method_getStack;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD(TAG, "JNI OnLoad...");
    m_java_vm = vm;
    return JNI_VERSION_1_6;
}

static jclass FindClass(JNIEnv* env, const char* desc, bool ret_global_ref = false) {
    jclass clazz = env->FindClass(desc);
    if (clazz == nullptr) {
        env->ExceptionClear();
        LOGE(TAG, "Cannot find class: %s", desc);
    }
    if (ret_global_ref) {
        clazz = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        if (clazz == nullptr) {
            LOGE(TAG, "Cannot create global ref for class: %s", desc);
        }
    }
    return clazz;
}

static jmethodID GetStaticMethodID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    jmethodID mid = env->GetStaticMethodID(clazz, name, sig);
    if (mid == nullptr) {
        env->ExceptionClear();
        LOGE(TAG, "Fail to get static method id of %s:%s", name, sig);
    }
    return mid;
}

// fixme 解偶 EglHook
JNIEXPORT jboolean Java_com_tencent_matrix_hook_HookManager_doPreHookInitializeNative(JNIEnv *env, jobject thiz) {
    m_class_HookManager = FindClass(env, "com/tencent/matrix/hook/HookManager", true);
    if (m_class_HookManager == nullptr) {
        return false;
    }
    auto jHookMgrCleaner = matrix::MakeScopedCleaner([env]() {
        if (m_class_HookManager != nullptr) {
            env->DeleteGlobalRef(m_class_HookManager);
            m_class_HookManager = nullptr;
        }
    });

    m_method_getStack = GetStaticMethodID(env, m_class_HookManager, "getStack", "()Ljava/lang/String;");
    if (m_method_getStack == nullptr) {
        return false;
    }
    auto getStackMethodCleaner = matrix::MakeScopedCleaner([]() {
        m_method_getStack = nullptr;
    });

    int ret = xhook_register(".*\\.so$", "__loader_android_dlopen_ext",
                             (void *) HANDLER_FUNC_NAME(__loader_android_dlopen_ext),
                             (void **) &ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext));
    if (ret != 0) {
        LOGE(TAG, "xhook_register with __loader_android_dlopen_ext failed, ret: %d", ret);
        return false;
    }

    pthread_ext_init();

    getStackMethodCleaner.Omit();
    jHookMgrCleaner.Omit();
    return true;
}

JNIEXPORT void JNICALL
Java_com_tencent_matrix_hook_HookManager_doFinalInitializeNative(JNIEnv *env, jobject thiz) {
    wechat_backtrace::notify_maps_changed();
    // This line only refresh xhook in matrix-hookcommon library now.
    int ret = xhook_refresh(0);
    if (ret != 0) {
        LOGE(TAG, "Fail to call xhook_refresh, ret: %d", ret);
    }
}

#ifdef __cplusplus
}
#endif

#undef TAG