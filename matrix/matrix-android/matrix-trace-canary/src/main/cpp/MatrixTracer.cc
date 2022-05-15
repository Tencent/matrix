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
#include <sys/utsname.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <xhook_ext.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <dlfcn.h>

#include "BacktraceDefine.h"
#include "Backtrace.h"
#include "cxxabi.h"

#include <cstdio>
#include <ctime>
#include <csignal>
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
#define KEY_VALID_FLAG (1 << 31)

#define HOOK_REQUEST_GROUPID_THREAD_PRIORITY_TRACE 0x01
#define HOOK_REQUEST_GROUPID_THREAD_PTHREAD_KEY_TRACE 0x13
#define HOOK_REQUEST_GROUPID_TOUCH_EVENT_TRACE 0x07
#define HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE 0x12

using namespace MatrixTracer;
using namespace std;

static std::optional<AnrDumper> sAnrDumper;
static bool isTraceWrite = false;
static bool fromMyPrintTrace = false;
static bool isHooking = false;
static std::string anrTracePathString;
static std::string printTracePathString;
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

    jmethodID AnrDetector_onNativeBacktraceDumped;

    jmethodID ThreadPriorityDetective_onMainThreadPriorityModified;
    jmethodID ThreadPriorityDetective_onMainThreadTimerSlackModified;
    jmethodID ThreadPriorityDetective_pthreadKeyCallback;

    jmethodID TouchEventLagTracer_onTouchEvenLag;
    jmethodID TouchEventLagTracer_onTouchEvenLagDumpTrace;
} gJ;

int (*original_setpriority)(int __which, id_t __who, int __priority);
int my_setpriority(int __which, id_t __who, int __priority) {
    if ((__who == 0 && getpid() == gettid()) || __who == getpid()) {
        int priorityBefore = getpriority(__which, __who);
        JNIEnv *env = JniInvocation::getEnv();
        env->CallStaticVoidMethod(gJ.ThreadPriorityDetective, gJ.ThreadPriorityDetective_onMainThreadPriorityModified, priorityBefore, __priority);
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
                targetFilePath = printTracePathString;
            } else {
                targetFilePath = anrTracePathString;
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

void pthreadKeyCallback(int type, int ret, int keySeq, const char* soName, const char* backtrace) {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return;

    jstring soNameJS = env->NewStringUTF(soName);
    jstring nativeBacktraceJS = env->NewStringUTF(backtrace);

    env->CallStaticVoidMethod(gJ.ThreadPriorityDetective, gJ.ThreadPriorityDetective_pthreadKeyCallback, type, ret, keySeq, soNameJS, nativeBacktraceJS);
    env->DeleteLocalRef(soNameJS);
    env->DeleteLocalRef(nativeBacktraceJS);
}

void makeNativeStack(wechat_backtrace::Backtrace* backtrace, char *&stack) {
    std::string caller_so_name;
    std::stringstream full_stack_builder;
    std::stringstream brief_stack_builder;
    std::string last_so_name;
    int index = 0;
    auto _callback = [&](wechat_backtrace::FrameDetail it) {
        std::string so_name = it.map_name;

        char *demangled_name = nullptr;

        demangled_name = abi::__cxa_demangle(it.function_name, nullptr, 0, &status);

        if (strstr(it.map_name, "libtrace-canary.so") || strstr(it.map_name, "libwechatbacktrace.so")) {
            return;
        }

        full_stack_builder
                << "#" << std::dec << (index++)
                << " pc " << std::hex << it.rel_pc << " "
                << it.map_name
                << " ("
                << (demangled_name ? demangled_name : "null")
                << ")"
                << std::endl;
        if (last_so_name != it.map_name) {
            last_so_name = it.map_name;
            brief_stack_builder << it.map_name << ";";
        }

        brief_stack_builder << std::hex << it.rel_pc << ";";

        if (demangled_name) {
            free(demangled_name);
        }
    };

    wechat_backtrace::restore_frame_detail(backtrace->frames.get(), backtrace->frame_size,
                                           _callback);

    stack = new char[full_stack_builder.str().size() + 1];
    strcpy(stack, full_stack_builder.str().c_str());
}

static const char* getNativeBacktrace() {
    wechat_backtrace::Backtrace *backtracePrt;

    wechat_backtrace::Backtrace backtrace_zero = BACKTRACE_INITIALIZER(
            16);


    backtracePrt = new wechat_backtrace::Backtrace;
    backtracePrt->max_frames = backtrace_zero.max_frames;
    backtracePrt->frame_size = backtrace_zero.frame_size;
    backtracePrt->frames = backtrace_zero.frames;

    wechat_backtrace::unwind_adapter(backtracePrt->frames.get(), backtracePrt->max_frames,
                                     backtracePrt->frame_size);

    char* nativeStack;
    makeNativeStack(backtracePrt, nativeStack);
    return nativeStack;
}

int (*original_pthread_key_create)(pthread_key_t *key, void (*destructor)(void*));
int my_pthread_key_create(pthread_key_t *key, void (*destructor)(void*)) {
    int ret = original_pthread_key_create(key, destructor);
    int keySeq = *key & ~KEY_VALID_FLAG;

    void * __caller_addr = __builtin_return_address(0);
    Dl_info dl_info;
    dladdr(__caller_addr, &dl_info);
    const char* soName = dl_info.dli_fname;
    if (!strstr(soName, "libc.so")) {
        pthreadKeyCallback(0, ret, keySeq, soName, getNativeBacktrace());
    }
    return ret;
}

int (*original_pthread_key_delete)(pthread_key_t key);
int my_pthread_key_delete(pthread_key_t key) {
    int ret = original_pthread_key_delete(key);
    int keySeq = key & ~KEY_VALID_FLAG;
    void * __caller_addr = __builtin_return_address(0);
    Dl_info dl_info;
    dladdr(__caller_addr, &dl_info);
    const char* soName = dl_info.dli_fname;
    if (!strstr(soName, "libc.so")) {
        pthreadKeyCallback(1, ret, keySeq, soName, getNativeBacktrace());
    }
    return 0;
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

bool nativeBacktraceDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onNativeBacktraceDumped);
    std::string to;
    std::ofstream outfile;
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
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libcutils\\.so$",
                               "connect", (void *) my_connect, (void **) (&original_connect));
    } else {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libart\\.so$",
                               "open", (void *) my_open, (void **) (&original_open));
    }

    if (apiLevel >= 30 || apiLevel == 25 || apiLevel == 24) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libc\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    } else if (apiLevel == 29) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libbase\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    } else {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_ANR_DUMP_TRACE, ".*libart\\.so$",
                               "write", (void *) my_write, (void **) (&original_write));
    }

    xhook_refresh(true);
}

void unHookAnrTraceWrite() {
    isHooking = false;
}

static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char* anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char* printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    anrTracePathString = std::string(anrTracePathChar);
    printTracePathString = std::string(printTracePathChar);
    sAnrDumper.emplace(anrTracePathChar, printTracePathChar);
}

static void nativeFreeSignalAnrDetective(JNIEnv *env, jclass) {
    sAnrDumper.reset();
}

static int nativeGetPthreadKeySeq(JNIEnv *env, jclass) {
    pthread_key_t key;
    pthread_key_create(&key, nullptr);
    int key_seq = key & ~KEY_VALID_FLAG;
    pthread_key_delete(key);
    return key_seq;
}

static void nativeInitThreadHook(JNIEnv *env, jclass, jint priority, jint pthreadKey) {
    if (priority == 1) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PRIORITY_TRACE, ".*\\.so$", "setpriority",
                               (void *) my_setpriority, (void **) (&original_setpriority));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PRIORITY_TRACE, ".*\\.so$", "prctl",
                               (void *) my_prctl, (void **) (&original_prctl));
    }

    if (pthreadKey == 1) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PTHREAD_KEY_TRACE, ".*\\.so$", "pthread_key_create",
                               (void *) my_pthread_key_create, (void **) (&original_pthread_key_create));

        xhook_grouped_register(HOOK_REQUEST_GROUPID_THREAD_PTHREAD_KEY_TRACE, ".*\\.so$", "pthread_key_delete",
                               (void *) my_pthread_key_delete, (void **) (&original_pthread_key_delete));


        xhook_export_symtable_hook("libc.so", "pthread_key_create",
                                   (void *) my_pthread_key_create, (void **) (&original_pthread_key_create));
        xhook_export_symtable_hook("libc.so", "pthread_key_delete",
                                   (void *) my_pthread_key_delete, (void **) (&original_pthread_key_delete));
    }

    if (pthreadKey + pthreadKey > 0) {
        xhook_enable_sigsegv_protection(0);
        xhook_refresh(0);
    }

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
        {"nativeInitThreadHook", "(II)V", (void *) nativeInitThreadHook},
        {"nativeGetPthreadKeySeq", "()I", (void *) nativeGetPthreadKeySeq},
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

    gJ.AnrDetector_onNativeBacktraceDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onNativeBacktraceDumped", "()V");


    if (env->RegisterNatives(
            anrDetectiveCls, ANR_METHODS, static_cast<jint>(NELEM(ANR_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(anrDetectiveCls);


    jclass threadPriorityDetectiveCls = env->FindClass("com/tencent/matrix/trace/tracer/ThreadTracer");

    jclass touchEventLagTracerCls = env->FindClass("com/tencent/matrix/trace/tracer/TouchEventLagTracer");

    if (!threadPriorityDetectiveCls || !touchEventLagTracerCls)
        return -1;
    gJ.ThreadPriorityDetective = static_cast<jclass>(env->NewGlobalRef(threadPriorityDetectiveCls));
    gJ.TouchEventLagTracer = static_cast<jclass>(env->NewGlobalRef(touchEventLagTracerCls));


    gJ.ThreadPriorityDetective_onMainThreadPriorityModified =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "onMainThreadPriorityModified", "(II)V");

    gJ.ThreadPriorityDetective_pthreadKeyCallback =
            env->GetStaticMethodID(threadPriorityDetectiveCls, "pthreadKeyCallback", "(IIILjava/lang/String;Ljava/lang/String;)V");

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
