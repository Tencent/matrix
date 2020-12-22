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

#include "QuickenUnwinder.h"
#include "QuickenJNI.h"
#include "Log.h"

#include "android-base/macros.h"

#define WECHAT_QUICKEN_UNWIND_TAG "QuickenUnwind"

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

    static void JNI_ConsumeRequestedQut(JNIEnv *env, jclass clazz) {
        (void) env;
        (void) clazz;
        ConsumeRequestingQut();
    }

    static void JNI_WarmUp(JNIEnv *env, jclass clazz, jstring sopath_jstr) {
        (void) clazz;
        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        GenerateQutForLibrary(sopath);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
    }

    static void JNI_Statistic(JNIEnv *env, jclass clazz, jstring sopath_jstr) {
        (void) clazz;
        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
//        StatisticWeChatQuickenUnwindTable(sopath);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
    }

    static JNINativeMethod g_qut_methods[] = {
            {"setPackageName",      "(Ljava/lang/String;)V", (void *) JNI_SetPackageName},
            {"setSavingPath",       "(Ljava/lang/String;)V", (void *) JNI_SetSavingPath},
            {"setWarmedUp",         "(Z)V",                  (void *) JNI_SetWarmedUp},
            {"consumeRequestedQut", "()V",                   (void *) JNI_ConsumeRequestedQut},
            {"warmUp",              "(Ljava/lang/String;)V", (void *) JNI_WarmUp},
            {"statistic",           "(Ljava/lang/String;)V", (void *) JNI_Statistic},
    };

    static jclass JNIClass_QuickenUnwinderNative = nullptr;
    static jmethodID JNIMethod_RequestQutGenerate = nullptr;
    static JavaVM *CurrentJavaVM = nullptr;

//    inline static JNIEnv *GetEnv() {
//        if (CurrentJavaVM) {
//            JNIEnv *currentEnv = nullptr;
//            auto ret = CurrentJavaVM->GetEnv(reinterpret_cast<void **>(&currentEnv),
//                                             JNI_VERSION_1_6);
//            if (ret == JNI_OK) {
//                return currentEnv;
//            }
//        }
//        return nullptr;
//    }

    static int RegisterQutJNINativeMethods(JNIEnv *env) {

        static const char *cls_name = "com/tencent/components/backtrace/WeChatBacktraceNative";
        jclass clazz = env->FindClass(cls_name);
        if (!clazz) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "Find Class %s failed.", cls_name);
            return -1;
        }
        JNIClass_QuickenUnwinderNative = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        int ret = env->RegisterNatives(JNIClass_QuickenUnwinderNative, g_qut_methods,
                                       sizeof(g_qut_methods) / sizeof(g_qut_methods[0]));

        JNIMethod_RequestQutGenerate = env->GetStaticMethodID(JNIClass_QuickenUnwinderNative,
                                                              "requestQutGenerate", "()V");
        if (!JNIMethod_RequestQutGenerate) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "requestQutGenerate() method not found.");
        }
        return ret;
    }

//    static inline int QutJNIOnLoaded(JavaVM *vm, JNIEnv *env) {
//        CurrentJavaVM = vm;
//        return RegisterQutJNINativeMethods(env);
//    }

    void InvokeJava_RequestQutGenerate() {

        if (JNIClass_QuickenUnwinderNative == nullptr || JNIMethod_RequestQutGenerate == nullptr) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "RegisterQutJNINativeMethods did not be run?");
            return;
        }

        JNIEnv *env = nullptr;
        auto ret = CurrentJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (ret != JNI_OK || env == nullptr) {
            return;
        }

        env->CallStaticVoidMethod(JNIClass_QuickenUnwinderNative, JNIMethod_RequestQutGenerate);
    }

    QUT_EXTERN_C_BLOCK_END
} // namespace wechat_backtrace

QUT_EXTERN_C_BLOCK

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {

    (void) reserved;

    LOGD(WECHAT_QUICKEN_UNWIND_TAG, "JNI OnLoad...");

    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    wechat_backtrace::CurrentJavaVM = vm;
    if (env) {
        if (wechat_backtrace::RegisterQutJNINativeMethods(env) != 0) {
            LOGE(WECHAT_QUICKEN_UNWIND_TAG, "Register Quicken Unwinder JNINativeMethods Failed.");
        }
    }
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    // no implementation
    (void) vm;
    (void) reserved;
}

QUT_EXTERN_C_BLOCK_END