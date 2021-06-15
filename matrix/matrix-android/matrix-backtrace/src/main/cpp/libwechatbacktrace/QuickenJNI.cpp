/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
#include <QuickenInterface.h>

#include "PthreadExt.h"
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

    static jboolean
    JNI_WarmUp(JNIEnv *env, jclass clazz, jstring sopath_jstr, jint elf_start_offset,
               jboolean only_save_file) {
        (void) clazz;

        jboolean ret = JNI_TRUE;
        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        if (!GenerateQutForLibrary(sopath, (uint64_t) elf_start_offset, only_save_file)) {
            ret = JNI_FALSE;
        }
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
        return ret;
    }

    static void
    JNI_NotifyWarmedUp(JNIEnv *env, jclass clazz, jstring sopath_jstr, jint elf_start_offset) {
        (void) clazz;

        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        NotifyWarmedUpQut(sopath, (uint64_t) elf_start_offset);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);
    }

    static jboolean
    JNI_TestLoadQut(JNIEnv *env, jclass clazz, jstring sopath_jstr, jint elf_start_offset) {

        (void) clazz;

        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);
        bool ret = TestLoadQut(sopath, (uint64_t) elf_start_offset);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);

        return ret ? JNI_TRUE : JNI_FALSE;
    }

    static void JNI_SetBacktraceMode(JNIEnv *env, jclass clazz, jint mode) {
        (void) env;
        (void) clazz;

        if (mode < FramePointer || mode > DwarfBased) return;

        set_backtrace_mode(static_cast<BacktraceMode>(mode));
    }

    static void JNI_SetQuickenAlwaysOn(JNIEnv *env, jclass clazz, jboolean enable) {
        (void) env;
        (void) clazz;

        set_quicken_always_enable(enable);
    }

    static void JNI_SetImmediateGeneration(JNIEnv *env, jclass clazz, jboolean immediate) {
        (void) env;
        (void) clazz;

        if (immediate) {
            QuickenInterface::SetQuickenGenerateDelegate(wechat_backtrace::GenerateQutForLibrary);
        } else {
            QuickenInterface::SetQuickenGenerateDelegate(nullptr);
        }
    }

    static jintArray JNI_Statistic(JNIEnv *env, jclass clazz, jstring sopath_jstr) {
        (void) clazz;

        const char *sopath = env->GetStringUTFChars(sopath_jstr, 0);

        std::vector<uint32_t> processed_result;
        StatisticWeChatQuickenUnwindTable(sopath, processed_result);
        env->ReleaseStringUTFChars(sopath_jstr, sopath);

        size_t array_size = processed_result.size();
        jintArray result = env->NewIntArray(array_size);
        if (array_size > 0) {
            env->SetIntArrayRegion(result, 0, processed_result.size(),
                                   (jint *) &processed_result[0]);
        }

        return result;
    }

    extern "C" int init_xlog_logger(const char *xlog_so_path);
    extern "C" int xlog_vlogger(int log_level, const char *tag, const char *format, ...);

    static void
    JNI_EnableLogger(JNIEnv *env, jclass clazz, jboolean enable) {
        (void) env;
        (void) clazz;
        enable_backtrace_logger(enable);
    }

    static void
    JNI_SetXLogger(JNIEnv *env, jclass clazz, jstring xlog_so_path) {
        (void) clazz;

        if (!xlog_so_path) {
            wechat_backtrace::internal_init_logger(
                    reinterpret_cast<internal_logger_func>(__android_log_vprint));
            return;
        }
        const char *xlog_sopath = env->GetStringUTFChars(xlog_so_path, 0);
        size_t size = env->GetStringUTFLength(xlog_so_path);
        if (size > 0 && wechat_backtrace::init_xlog_logger(xlog_sopath) == 0) {
            set_xlog_logger_path(xlog_sopath, size);
            wechat_backtrace::internal_init_logger(
                    reinterpret_cast<internal_logger_func>(wechat_backtrace::xlog_vlogger));
        } else {
            set_xlog_logger_path(nullptr, 0);
            wechat_backtrace::internal_init_logger(
                    reinterpret_cast<internal_logger_func>(__android_log_vprint));
        }
        env->ReleaseStringUTFChars(xlog_so_path, xlog_sopath);

    }

    static jstring
    JNI_GetXLogger(JNIEnv *env, jclass clazz) {
        (void) clazz;
        char * xlog_logger = get_xlog_logger_path();
        if (xlog_logger == nullptr) {
            return env->NewStringUTF("");
        }
        return env->NewStringUTF(xlog_logger);

    }

    static JNINativeMethod g_qut_methods[] = {
            {"setPackageName",      "(Ljava/lang/String;)V",   (void *) JNI_SetPackageName},
            {"setSavingPath",       "(Ljava/lang/String;)V",   (void *) JNI_SetSavingPath},
            {"setWarmedUp",         "(Z)V",                    (void *) JNI_SetWarmedUp},
            {"consumeRequestedQut", "()[Ljava/lang/String;",   (void *) JNI_ConsumeRequestedQut},
            {"warmUp",              "(Ljava/lang/String;IZ)Z", (void *) JNI_WarmUp},
            {"setBacktraceMode",    "(I)V",                    (void *) JNI_SetBacktraceMode},
            {"setQuickenAlwaysOn",  "(Z)V",                    (void *) JNI_SetQuickenAlwaysOn},
            {"statistic",           "(Ljava/lang/String;)[I",  (void *) JNI_Statistic},
            {"immediateGeneration", "(Z)V",                    (void *) JNI_SetImmediateGeneration},
            {"notifyWarmedUp",      "(Ljava/lang/String;I)V",  (void *) JNI_NotifyWarmedUp},
            {"enableLogger",        "(Z)V",                    (void *) JNI_EnableLogger},
            {"testLoadQut",         "(Ljava/lang/String;I)Z",  (void *) JNI_TestLoadQut},
    };

    static JNINativeMethod g_xlog_methods[] = {
            {"setXLoggerNative", "(Ljava/lang/String;)V", (void *) JNI_SetXLogger},
            {"getXLoggerNative", "()Ljava/lang/String;", (void *) JNI_GetXLogger},
    };

    static jclass JNIClass_WeChatBacktraceNative = nullptr;
    static jclass JNIClass_XLogNative = nullptr;
    static jmethodID JNIMethod_RequestQutGenerate = nullptr;
    static JavaVM *CurrentJavaVM = nullptr;

    static int RegisterQutJNINativeMethods(JNIEnv *env) {

        static const char *cls_name = "com/tencent/matrix/backtrace/WeChatBacktraceNative";
        jclass clazz = env->FindClass(cls_name);
        if (!clazz) {
            QUT_LOG("Find Class %s failed.", cls_name);
            return -1;
        }
        JNIClass_WeChatBacktraceNative = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        int ret = env->RegisterNatives(JNIClass_WeChatBacktraceNative, g_qut_methods,
                                       sizeof(g_qut_methods) / sizeof(g_qut_methods[0]));

        JNIMethod_RequestQutGenerate = env->GetStaticMethodID(JNIClass_WeChatBacktraceNative,
                                                              "requestQutGenerate", "()V");
        if (!JNIMethod_RequestQutGenerate) {
            QUT_LOG("requestQutGenerate() method not found.");
            return -2;
        }
        return ret;
    }

    static int RegisterXLogNativeMethods(JNIEnv *env) {

        static const char *cls_name = "com/tencent/matrix/xlog/XLogNative";
        jclass clazz = env->FindClass(cls_name);
        if (!clazz) {
            QUT_LOG("Find Class %s failed.", cls_name);
            return -1;
        }
        JNIClass_XLogNative = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
        int ret = env->RegisterNatives(JNIClass_XLogNative, g_xlog_methods,
                                       sizeof(g_xlog_methods) / sizeof(g_xlog_methods[0]));

        return ret;
    }

    void InvokeJava_RequestQutGenerate() {

        if (JNIClass_WeChatBacktraceNative == nullptr || JNIMethod_RequestQutGenerate == nullptr) {
            QUT_LOG("RegisterQutJNINativeMethods did not be run?");
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

    QUT_LOG("JNI OnLoad...");

    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    wechat_backtrace::CurrentJavaVM = vm;
    if (env) {
        if (wechat_backtrace::RegisterQutJNINativeMethods(env) != 0) {
            QUT_LOG("Register Quicken Unwinder JNINativeMethods Failed.");
        }

        if (wechat_backtrace::RegisterXLogNativeMethods(env) != 0) {
            QUT_LOG("Register XLog JNINativeMethods Failed.");
        }
    }

    BACKTRACE_FUNC_WRAPPER(pthread_ext_init)();

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    (void) vm;
    (void) reserved;
    // no implementation
}

QUT_EXTERN_C_BLOCK_END