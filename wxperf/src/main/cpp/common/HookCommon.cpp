//
// Created by Yves on 2020-03-17.
//

#include <jni.h>
#include <xhook.h>
#include <sys/stat.h>
#include "HookCommon.h"
#include "log.h"
#include "StackTrace.h"
#include "JNICommon.h"

#ifdef __cplusplus
extern "C" {
#endif

std::vector<dlopen_callback_t> m_dlopen_callbacks;
std::vector<hook_init_callback_t> m_init_callbacks;

DEFINE_HOOK_FUN(void *, __loader_android_dlopen_ext, const char *__file_name,
                int                                             __flag,
                const void                                      *__extinfo,
                const void                                      *__caller_addr) {
    void *ret = (*ORIGINAL_FUNC_NAME(__loader_android_dlopen_ext))(__file_name, __flag, __extinfo,
                                                                   __caller_addr);

    LOGD("Yves-debug", "call into dlopen hook");

    for (auto &callback : m_dlopen_callbacks) {
        callback(__file_name);
    }

    xhook_refresh(false);

    return ret;
}

static void hook_common_init() {
    for (auto &callback : m_init_callbacks) {
        callback();
    }
}

void add_dlopen_hook_callback(dlopen_callback_t __callback) {
    m_dlopen_callbacks.push_back(__callback);
}

void add_hook_init_callback(hook_init_callback_t __callback) {
    m_init_callbacks.push_back(__callback);
}

void test_log_to_file(const char *ch) {
    const char *dir = "/sdcard/Android/data/com.tencent.mm/MicroMsg/Diagnostic";
    const char *path = "/sdcard/Android/data/com.tencent.mm/MicroMsg/Diagnostic/log";

    mkdir(dir, S_IRWXU|S_IRWXG|S_IROTH);

    FILE *log_file = fopen(path, "a+");

    if (!log_file) {
        return;
    }

    fprintf(log_file, "%p:%s\n", ch, ch);

    fflush(log_file);
    fclose(log_file);

}

bool get_java_stacktrace(char *__stack, size_t __size) {
    JNIEnv *env = NULL;
    bool attached = false;

    if (!__stack) {
        return false;
    }

    if (m_java_vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (m_java_vm->AttachCurrentThread(&env, NULL) == JNI_OK) {
            attached = true;
        } else {
            return false;
        }
    }

    if (env != NULL) {
        LOGD("Yves-debug", "get_java_stacktrace call");

        jstring j_stacktrace = (jstring) env->CallStaticObjectMethod(m_class_HookManager, m_method_getStack);

        LOGD("Yves-debug", "get_java_stacktrace called");
        const char *stack = env->GetStringUTFChars(j_stacktrace, NULL);
        test_log_to_file(stack);
        memcpy(__stack, stack, __size - 1);
        __stack[__size - 1] = '\0';
        env->ReleaseStringUTFChars(j_stacktrace, stack);
        if (attached) {
            m_java_vm->DetachCurrentThread();
        }

        return true;
    }

    strncpy(__stack, "  null", __size);
    if (attached) {
        m_java_vm->DetachCurrentThread();
    }
    return false;
}

JNIEXPORT jint JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookRefreshNative(JNIEnv *env, jobject thiz,
                                                                  jboolean async) {
    hook_common_init();
    unwindstack::update_maps();
    return xhook_refresh(async);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookEnableDebugNative(JNIEnv *env, jobject thiz,
                                                                      jboolean flag) {
    xhook_enable_debug(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookEnableSigSegvProtectionNative(JNIEnv *env,
                                                                                  jobject thiz,
                                                                                  jboolean flag) {
    xhook_enable_sigsegv_protection(flag);
}

JNIEXPORT void JNICALL
Java_com_tencent_mm_performance_jni_HookManager_xhookClearNative(JNIEnv *env, jobject thiz) {
    xhook_clear();
}

#ifdef __cplusplus
}
#endif