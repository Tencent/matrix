#include <stdint.h>
#include <bits/pthread_types.h>
#include <cstdlib>
#include <pthread.h>
#include <android-base/strings.h>
#include <include/unwindstack/DwarfError.h>
#include <dlfcn.h>
#include <cinttypes>
#include <sys/mman.h>
#include <fcntl.h>
#include <android-base/unique_fd.h>
#include <sys/stat.h>
#include <QuickenTableManager.h>
#include <jni.h>
#include <Backtrace.h>

#include "QuickenUnwinder.h"
#include "QuickenJNI.h"
#include "Log.h"

#include "android-base/macros.h"

#define WECHAT_BACKTRACE_TAG "WeChatBacktrace"

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    using namespace std;
    using namespace unwindstack;

    static void JNI_SetPackageName(JNIEnv *env, jclass clazz, jstring package_name_jstr) {
        (void) clazz;
        const char *package_name = env->GetStringUTFChars(package_name_jstr, 0);
        QuickenTableManager::SetPackageName(package_name);
        env->ReleaseStringUTFChars(package_name_jstr, package_name);
    }

    static void JNI_SetSavingPath(JNIEnv *env, jclass clazz, jstring saving_path_jstr) {
        (void) clazz;
        const char *saving_path = env->GetStringUTFChars(saving_path_jstr, 0);
        QuickenTableManager::SetSavingPath(saving_path);
        env->ReleaseStringUTFChars(saving_path_jstr, saving_path);
    }

    static void JNI_SetWarmedUp(JNIEnv *env, jclass clazz, jboolean warmed_up) {
        (void) env;
        (void) clazz;
        QuickenTableManager::getInstance().WarmUp(warmed_up == JNI_TRUE);
    }

    static jobjectArray JNI_ConsumeRequestedQut(JNIEnv *env, jclass clazz) {
        (void) env;
        (void) clazz;
        vector<string> consumed = ConsumeRequestingQut();

        jobjectArray result = (jobjectArray)
                env->NewObjectArray(consumed.size(), env->FindClass("java/lang/String"),
                                    env->NewStringUTF(""));

        auto it = consumed.begin();
        size_t i = 0;
        while (it != consumed.end()) {
            env->SetObjectArrayElement(result, i, env->NewStringUTF(it->c_str()));
            i++;
            it++;
        }

        return result;
    }

    static void JNI_WarmUp(JNIEnv *env, jclass clazz, jstring sopath_jstr) {
        (void) clazz;
        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        GenerateQutForLibrary(sopath, 0);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
    }

    static void JNI_SetBacktraceMode(JNIEnv *env, jclass clazz, jint mode) {
        (void) env;
        (void) clazz;

        if (mode < FramePointer || mode > DwarfBased) return;

        SetBacktraceMode(static_cast<BacktraceMode>(mode));
    }

    static void JNI_Statistic(JNIEnv *env, jclass clazz, jstring sopath_jstr) {
        (void) clazz;
        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        StatisticWeChatQuickenUnwindTable(sopath);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
    }

    static JNINativeMethod g_qut_methods[] = {
            {"setPackageName",      "(Ljava/lang/String;)V", (void *) JNI_SetPackageName},
            {"setSavingPath",       "(Ljava/lang/String;)V", (void *) JNI_SetSavingPath},
            {"setWarmedUp",         "(Z)V",                  (void *) JNI_SetWarmedUp},
            {"consumeRequestedQut", "()[Ljava/lang/String;", (void *) JNI_ConsumeRequestedQut},
            {"warmUp",              "(Ljava/lang/String;)V", (void *) JNI_WarmUp},
            {"setBacktraceMode",    "(I)V",                  (void *) JNI_SetBacktraceMode},
            {"statistic",           "(Ljava/lang/String;)V", (void *) JNI_Statistic},
    };

    static jclass JNIClass_WeChatBacktraceNative = nullptr;
    static jmethodID JNIMethod_RequestQutGenerate = nullptr;
    static JavaVM *CurrentJavaVM = nullptr;

    static int RegisterQutJNINativeMethods(JNIEnv *env) {

        static const char *cls_name = "com/tencent/components/backtrace/WeChatBacktraceNative";
        jclass clazz = env->FindClass(cls_name);
        if (!clazz) {
            LOGE(WECHAT_BACKTRACE_TAG, "Find Class %s failed.", cls_name);
            return -1;
        }
        JNIClass_WeChatBacktraceNative = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        int ret = env->RegisterNatives(JNIClass_WeChatBacktraceNative, g_qut_methods,
                                       sizeof(g_qut_methods) / sizeof(g_qut_methods[0]));

        JNIMethod_RequestQutGenerate = env->GetStaticMethodID(JNIClass_WeChatBacktraceNative,
                                                              "requestQutGenerate", "()V");
        if (!JNIMethod_RequestQutGenerate) {
            LOGE(WECHAT_BACKTRACE_TAG, "requestQutGenerate() method not found.");
            return -2;
        }
        return ret;
    }

    void InvokeJava_RequestQutGenerate() {

        if (JNIClass_WeChatBacktraceNative == nullptr || JNIMethod_RequestQutGenerate == nullptr) {
            LOGE(WECHAT_BACKTRACE_TAG, "RegisterQutJNINativeMethods did not be run?");
            return;
        }

        JNIEnv *env = nullptr;
        auto ret = CurrentJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (ret != JNI_OK || env == nullptr) {
            return;
        }

        env->CallStaticVoidMethod(JNIClass_WeChatBacktraceNative, JNIMethod_RequestQutGenerate);
    }

    QUT_EXTERN_C_BLOCK_END
} // namespace wechat_backtrace

QUT_EXTERN_C_BLOCK

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {

    (void) reserved;

    LOGD(WECHAT_BACKTRACE_TAG, "JNI OnLoad...");

    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    wechat_backtrace::CurrentJavaVM = vm;
    if (env) {
        if (wechat_backtrace::RegisterQutJNINativeMethods(env) != 0) {
            LOGE(WECHAT_BACKTRACE_TAG, "Register Quicken Unwinder JNINativeMethods Failed.");
        }
    }
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    (void) vm;
    (void) reserved;
    // no implementation
}

QUT_EXTERN_C_BLOCK_END