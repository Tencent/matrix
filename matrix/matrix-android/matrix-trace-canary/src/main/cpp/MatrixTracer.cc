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
#include <xhook_ext.h>
#include <linux/prctl.h>
#include <sys/prctl.h>

#include <thread>
#include <memory>
#include <string>
#include <optional>
#include <sstream>
#include <fstream>

#include "Logging.h"
#include "Support.h"
#include "nativehelper/managed_jnienv.h"
#include "nativehelper/scoped_utf_chars.h"
#include "nativehelper/scoped_local_ref.h"
#include "AnrDumper.h"
#include "TouchEventTracer.h"

#define PROP_VALUE_MAX  92
#define PROP_SDK_NAME "ro.build.version.sdk"
#define HOOK_CONNECT_PATH "/dev/socket/tombstoned_java_trace"
#define HOOK_OPEN_PATH "/data/anr/traces.txt"
#define VALIDATE_RET 50

#define HOOK_REQUEST_GROUPID_THREAD_PRIO_TRACE 0x01
#define HOOK_REQUEST_GROUPID_TOUCH_EVENT_TRACE 0x07

using namespace MatrixTracer;
using namespace std;

static std::optional<AnrDumper> sAnrDumper;
static bool isTraceWrite = false;
static bool fromMyPrintTrace = false;
static bool isHooking = false;
static std::string anrTracePathstring;
static std::string printTracePathstring;
static int signalCatcherTid;
static int currentTouchFd;
static bool inputHasSent;

static struct StacktraceJNI {
    jclass AnrDetective;
    jclass ThreadPriorityDetective;
    jclass TouchEventLagTracer;
    jmethodID AnrDetector_onANRDumped;
    jmethodID AnrDetector_onANRDumpTrace;
    jmethodID AnrDetector_onPrintTrace;

    jmethodID ThreadPriorityDetective_onMainThreadPriorityModified;
    jmethodID ThreadPriorityDetective_onMainThreadTimerSlackModified;

    jmethodID TouchEventLagTracer_onTouchEvenLag;
    jmethodID TouchEventLagTracer_onTouchEvenLagDumpTrace;
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


void writeAnr(const std::string& content, const std::string &filePath) {
    unHookAnrTraceWrite();
    std::stringstream stringStream(content);
    std::string to;
    std::ofstream outfile;
    outfile.open(filePath);
    outfile << content;
}

int (*original_connect)(int __fd, const struct sockaddr* __addr, socklen_t __addr_length);
int my_connect(int __fd, const struct sockaddr* __addr, socklen_t __addr_length) {
    if (__addr!= nullptr) {
        if (strcmp(__addr->sa_data, HOOK_CONNECT_PATH) == 0) {
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    return original_connect(__fd, __addr, __addr_length);
}

int (*original_open)(const char *pathname, int flags, mode_t mode);

int my_open(const char *pathname, int flags, mode_t mode) {
    if (pathname!= nullptr) {
        if (strcmp(pathname, HOOK_OPEN_PATH) == 0) {
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    return original_open(pathname, flags, mode);
}

ssize_t (*original_write)(int fd, const void* const __pass_object_size0 buf, size_t count);
ssize_t my_write(int fd, const void* const buf, size_t count) {
    if(isTraceWrite && gettid() == signalCatcherTid) {
        isTraceWrite = false;
        signalCatcherTid = 0;
        if (buf != nullptr) {
            std::string targetFilePath;
            if (fromMyPrintTrace) {
                targetFilePath = printTracePathstring;
            } else {
                targetFilePath = anrTracePathstring;
            }
            if (!targetFilePath.empty()) {
                char *content = (char *) buf;
                writeAnr(content, targetFilePath);
                if(!fromMyPrintTrace) {
                    anrDumpTraceCallback();
                } else {
                    printTraceCallback();
                }
                fromMyPrintTrace = false;
            }
        }
    }
    return original_write(fd, buf, count);
}

void onTouchEventLag(int fd) {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return;
    env->CallStaticVoidMethod(gJ.TouchEventLagTracer, gJ.TouchEventLagTracer_onTouchEvenLag, fd);
}

void onTouchEventLagDumpTrace(int fd) {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return;
    env->CallStaticVoidMethod(gJ.TouchEventLagTracer, gJ.TouchEventLagTracer_onTouchEvenLagDumpTrace, fd);
}

ssize_t (*original_recvfrom)(int sockfd, void *buf, size_t len, int flags,
                             struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t my_recvfrom(int sockfd, void *buf, size_t len, int flags,
                    struct sockaddr *src_addr, socklen_t *addrlen) {
    long ret = original_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);

    if (currentTouchFd == sockfd && inputHasSent && ret > VALIDATE_RET) {
        TouchEventTracer::touchRecv(sockfd);
    }

    if (currentTouchFd != sockfd) {
        TouchEventTracer::touchSendFinish(sockfd);
    }

    if (ret > 0) {
        currentTouchFd = sockfd;
    } else if (ret == 0) {
        TouchEventTracer::touchSendFinish(sockfd);
    }
    return ret;
}

ssize_t (*original_sendto)(int sockfd, const void *buf, size_t len, int flags,
                           const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t my_sendto(int sockfd, const void *buf, size_t len, int flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen) {

    long ret = original_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    if (ret >= 0) {
        inputHasSent = true;
        TouchEventTracer::touchSendFinish(sockfd);
    }
    return ret;
}

bool anrDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumped);
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

int getApiLevel() {
    char buf[PROP_VALUE_MAX];
    int len = __system_property_get(PROP_SDK_NAME, buf);
    if (len <= 0)
        return 0;

    return atoi(buf);
}


void hookAnrTraceWrite(bool isSiUser) {
    int apiLevel = getApiLevel();
    if (apiLevel < 19) {
        return;
    }

    if (!fromMyPrintTrace && isSiUser) {
        return;
    }

    if (isHooking) {
        return;
    }

    isHooking = true;

    if (apiLevel >= 27) {
        void *libcutils_info = xhook_elf_open("/system/lib64/libcutils.so");
        if(!libcutils_info) {
            libcutils_info = xhook_elf_open("/system/lib/libcutils.so");
        }
        xhook_got_hook_symbol(libcutils_info, "connect", (void*) my_connect, (void**) (&original_connect));
    } else {
        void* libart_info = xhook_elf_open("libart.so");
        xhook_got_hook_symbol(libart_info, "open", (void*) my_open, (void**) (&original_open));
    }

    if (apiLevel >= 30 || apiLevel == 25 || apiLevel == 24) {
        void* libc_info = xhook_elf_open("libc.so");
        xhook_got_hook_symbol(libc_info, "write", (void*) my_write, (void**) (&original_write));
    } else if (apiLevel == 29) {
        void* libbase_info = xhook_elf_open("/system/lib64/libbase.so");
        if(!libbase_info) {
            libbase_info = xhook_elf_open("/system/lib/libbase.so");
        }
        xhook_got_hook_symbol(libbase_info, "write", (void*) my_write, (void**) (&original_write));
        xhook_elf_close(libbase_info);
    } else {
        void* libart_info = xhook_elf_open("libart.so");
        xhook_got_hook_symbol(libart_info, "write", (void*) my_write, (void**) (&original_write));
    }
}

void unHookAnrTraceWrite() {
    int apiLevel = getApiLevel();
    if (apiLevel >= 27) {
        void *libcutils_info = xhook_elf_open("/system/lib64/libcutils.so");
        xhook_got_hook_symbol(libcutils_info, "connect", (void*) original_connect, nullptr);
    } else {
        void* libart_info = xhook_elf_open("libart.so");
        xhook_got_hook_symbol(libart_info, "open", (void*) original_connect, nullptr);
    }

    if (apiLevel >= 30 || apiLevel == 25 || apiLevel ==24) {
        void* libc_info = xhook_elf_open("libc.so");
        xhook_got_hook_symbol(libc_info, "write", (void*) original_write, nullptr);
    } else if (apiLevel == 29) {
        void* libbase_info = xhook_elf_open("/system/lib64/libbase.so");
        xhook_got_hook_symbol(libbase_info, "write", (void*) original_write, nullptr);
    } else {
        void* libart_info = xhook_elf_open("libart.so");
        xhook_got_hook_symbol(libart_info, "write", (void*) original_write, nullptr);
    }
    isHooking = false;
}

static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char* anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char* printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    anrTracePathstring = std::string(anrTracePathChar);
    printTracePathstring = std::string(printTracePathChar);
    sAnrDumper.emplace(anrTracePathChar, printTracePathChar, anrDumpCallback);
}

static void nativeFreeSignalAnrDetective(JNIEnv *env, jclass) {
    sAnrDumper.reset();
}

static void nativeInitMainThreadPriorityDetective(JNIEnv *env, jclass) {
    xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PRIO_TRACE, ".*\\.so$", "setpriority",
            (void *) my_setpriority, (void **) (&original_setpriority));
    xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PRIO_TRACE, ".*\\.so$", "prctl",
            (void *) my_prctl, (void **) (&original_prctl));
    xhook_refresh(true);
}

static void nativeInitTouchEventLagDetective(JNIEnv *env, jclass, jint threshold) {
    xhook_grouped_register(HOOK_REQUEST_GROUPID_TOUCH_EVENT_TRACE, ".*libinput\\.so$", "__sendto_chk",
                           (void *) my_sendto, (void **) (&original_sendto));
    xhook_grouped_register(HOOK_REQUEST_GROUPID_TOUCH_EVENT_TRACE, ".*libinput\\.so$", "sendto",
                           (void *) my_sendto, (void **) (&original_sendto));
    xhook_grouped_register(HOOK_REQUEST_GROUPID_TOUCH_EVENT_TRACE, ".*libinput\\.so$", "recvfrom",
                           (void *) my_recvfrom, (void **) (&original_recvfrom));
    xhook_refresh(true);

    TouchEventTracer::start(threshold);

}
static void nativePrintTrace() {
    fromMyPrintTrace = true;
    kill(getpid(), SIGQUIT);
}

template <typename T, std::size_t sz>
static inline constexpr std::size_t NELEM(const T(&)[sz]) { return sz; }

static const JNINativeMethod ANR_METHODS[] = {
    {"nativeInitSignalAnrDetective", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) nativeInitSignalAnrDetective},
    {"nativeFreeSignalAnrDetective", "()V", (void *) nativeFreeSignalAnrDetective},
    {"nativePrintTrace", "()V", (void *) nativePrintTrace},
};

static const JNINativeMethod THREAD_PRIORITY_METHODS[] = {
        {"nativeInitMainThreadPriorityDetective", "()V", (void *) nativeInitMainThreadPriorityDetective},

};

static const JNINativeMethod TOUCH_EVENT_TRACE_METHODS[] = {
        {"nativeInitTouchEventLagDetective", "(I)V", (void *) nativeInitTouchEventLagDetective},

};

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
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumped", "()V");
    gJ.AnrDetector_onANRDumpTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumpTrace", "()V");
    gJ.AnrDetector_onPrintTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onPrintTrace", "()V");


    if (env->RegisterNatives(
            anrDetectiveCls, ANR_METHODS, static_cast<jint>(NELEM(ANR_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(anrDetectiveCls);


    jclass threadPriorityDetectiveCls = env->FindClass("com/tencent/matrix/trace/tracer/ThreadPriorityTracer");

    jclass touchEventLagTracerCls = env->FindClass("com/tencent/matrix/trace/tracer/TouchEventLagTracer");

    if (!threadPriorityDetectiveCls || !touchEventLagTracerCls)
        return -1;
    gJ.ThreadPriorityDetective = static_cast<jclass>(env->NewGlobalRef(threadPriorityDetectiveCls));
    gJ.TouchEventLagTracer = static_cast<jclass>(env->NewGlobalRef(touchEventLagTracerCls));


    gJ.ThreadPriorityDetective_onMainThreadPriorityModified =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "onMainThreadPriorityModified", "(I)V");
    gJ.ThreadPriorityDetective_onMainThreadTimerSlackModified =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "onMainThreadTimerSlackModified", "(J)V");

    gJ.TouchEventLagTracer_onTouchEvenLag =
            env->GetStaticMethodID(touchEventLagTracerCls, "onTouchEventLag", "(I)V");

    gJ.TouchEventLagTracer_onTouchEvenLagDumpTrace =
            env->GetStaticMethodID(touchEventLagTracerCls, "onTouchEventLagDumpTrace", "(I)V");

    if (env->RegisterNatives(
            threadPriorityDetectiveCls, THREAD_PRIORITY_METHODS, static_cast<jint>(NELEM(THREAD_PRIORITY_METHODS))) != 0)
        return -1;

    if (env->RegisterNatives(
            touchEventLagTracerCls, TOUCH_EVENT_TRACE_METHODS, static_cast<jint>(NELEM(TOUCH_EVENT_TRACE_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(threadPriorityDetectiveCls);
    env->DeleteLocalRef(touchEventLagTracerCls);

    return JNI_VERSION_1_6;
}   // namespace MatrixTracer
