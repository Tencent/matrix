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

// Author: leafjia@tencent.com
//
// MatrixTracer.cpp

#include "MatrixTracer.h"

#include <jni.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <signal.h>
#include <xhook.h>
#include <linux/prctl.h>
#include <sys/prctl.h>

#include <memory>
#include <string>
#include <optional>

#include "Logging.h"
#include "Support.h"
#include "nativehelper/managed_jnienv.h"
#include "nativehelper/scoped_utf_chars.h"
#include "nativehelper/scoped_local_ref.h"
#include "AnrDumper.h"

#define voidp void*

using namespace MatrixTracer;

static std::optional<AnrDumper> sAnrDumper;

static struct StacktraceJNI {
    jclass AnrDetective;
    jclass ThreadPriorityDetective;
    jmethodID AnrDetector_onANRDumped;
    jmethodID AnrDetector_onANRDumpTrace;
    jmethodID AnrDetector_onPrintTrace;

    jmethodID ThreadPriorityDetective_onMainThreadPriorityModified;
    jmethodID ThreadPriorityDetective_onMainThreadTimerSlackModified;
} gJ;

int (*original_setpriority)(int __which, id_t __who, int __priority);
int my_setpriority(int __which, id_t __who, int __priority) {

    if (__priority <= 0) {
        return original_setpriority(__which, __who, __priority);
    }
    if (__who == 0 && getpid() == gettid()) {
        JNIEnv *env = JniInvocation::getEnv();
        env->CallStaticVoidMethod(gJ.ThreadPriorityDetective, gJ.ThreadPriorityDetective_onMainThreadPriorityModified, __priority);
    } else if (__who == getpid()) {
        JNIEnv *env = JniInvocation::getEnv();
        env->CallStaticVoidMethod(gJ.ThreadPriorityDetective, gJ.ThreadPriorityDetective_onMainThreadPriorityModified, __priority);
    }

    return original_setpriority(__which, __who, __priority);
}

int (*original_prctl)(int option, unsigned long arg2, unsigned long arg3,
                 unsigned long arg4, unsigned long arg5);

int my_prctl(int option, unsigned long arg2, unsigned long arg3,
             unsigned long arg4, unsigned long arg5) {

    if(option == PR_SET_TIMERSLACK) {
        if (gettid()==getpid() && arg2 > 50000) {
            JNIEnv *env = JniInvocation::getEnv();
            env->CallStaticVoidMethod(gJ.ThreadPriorityDetective, gJ.ThreadPriorityDetective_onMainThreadTimerSlackModified, arg2);

        }
    }

    return original_prctl(option, arg2, arg3, arg4, arg5);
}



bool anrDumpCallback(int status, const char *nativeStack) {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    ScopedLocalRef<jstring> nativeStackStr(env);
    if (nativeStack)
        nativeStackStr.reset(env->NewStringUTF(nativeStack));
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumped,
                                                   status, nativeStackStr.get());
    return true;
}

bool anrDumpTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumpTrace);
    return true;
}

bool printTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onPrintTrace);
    return true;
}


static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char* anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char* printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    sAnrDumper.emplace(anrTracePathChar, printTracePathChar, anrDumpCallback);
}

static void nativeFreeSignalAnrDetective(JNIEnv *env, jclass) {
    sAnrDumper.reset();
}

static void nativeInitMainThreadPriorityDetective(JNIEnv *env, jclass) {
    xhook_register(".*\\.so$", "setpriority", (void *) my_setpriority, (void **) (&original_setpriority));
    xhook_register(".*\\.so$", "prctl", (void *) my_prctl, (void **) (&original_prctl));
    xhook_refresh(true);
}

static void nativePrintTrace() {
    sAnrDumper->doDump(false);
}


template <typename T, std::size_t sz>
static inline constexpr std::size_t NELEM(const T(&)[sz]) { return sz; }

static const JNINativeMethod ANR_METHODS[] = {
    {"nativeInitSignalAnrDetective", "(Ljava/lang/String;Ljava/lang/String;)V", (voidp) nativeInitSignalAnrDetective},
    {"nativeFreeSignalAnrDetective", "()V", (voidp) nativeFreeSignalAnrDetective},
    {"nativePrintTrace", "()V", (voidp) nativePrintTrace},
};

static const JNINativeMethod THREAD_PRIORITY_METHODS[] = {
        {"nativeInitMainThreadPriorityDetective", "()V", (voidp) nativeInitMainThreadPriorityDetective},
};


#ifndef NDEBUG
static void nativeTestSegv(JNIEnv *, jclass) {
    ALOGI("I'm going to die with SIGSEGV().");
}

static void nativeTestAbrt(JNIEnv *, jclass) {
    ALOGI("I'm going to die with abort().");
    abort();
}

#endif


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JniInvocation::init(vm);

    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
        return -1;

    jclass anrDetectiveCls = env->FindClass("com/tencent/matrix/trace/tracer/SignalAnrTracer");
    if (!anrDetectiveCls)
        return -1;
    gJ.AnrDetective = static_cast<jclass>(env->NewGlobalRef(anrDetectiveCls));
    gJ.AnrDetector_onANRDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumped", "(ILjava/lang/String;)V");
    gJ.AnrDetector_onANRDumpTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumpTrace", "()V");
    gJ.AnrDetector_onPrintTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onPrintTrace", "()V");


    if (env->RegisterNatives(
            anrDetectiveCls, ANR_METHODS, static_cast<jint>(NELEM(ANR_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(anrDetectiveCls);


    jclass threadPriorityDetectiveCls = env->FindClass("com/tencent/matrix/trace/tracer/ThreadPriorityTracer");
    if (!threadPriorityDetectiveCls)
        return -1;
    gJ.ThreadPriorityDetective = static_cast<jclass>(env->NewGlobalRef(threadPriorityDetectiveCls));
    gJ.ThreadPriorityDetective_onMainThreadPriorityModified =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "onMainThreadPriorityModified", "(I)V");
    gJ.ThreadPriorityDetective_onMainThreadTimerSlackModified =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "onMainThreadTimerSlackModified", "(J)V");


    if (env->RegisterNatives(
            threadPriorityDetectiveCls, THREAD_PRIORITY_METHODS, static_cast<jint>(NELEM(THREAD_PRIORITY_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(threadPriorityDetectiveCls);



    return JNI_VERSION_1_6;
}   // namespace MatrixTracer
