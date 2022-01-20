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
// MatrixTraffic.cc

#include "MatrixTraffic.h"

#include <TrafficCollector.h>
#include <jni.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <xhook_ext.h>
#include <util/managed_jnienv.h>

#include <thread>
#include <string>
#include <sstream>
#include <fstream>

#include "BacktraceDefine.h"
#include "Backtrace.h"
#include "cxxabi.h"

using namespace std;

#define HOOK_REQUEST_GROUPID_TRAFFIC 0x06

using namespace MatrixTraffic;
static bool HOOKED = false;
static bool sDumpNativeBackTrace = false;

static struct StacktraceJNI {
    jclass TrafficPlugin;
    jmethodID TrafficPlugin_setFdStackTrace;
} gJ;


void makeNativeStack(wechat_backtrace::Backtrace* backtrace, char *&stack) {
    std::string caller_so_name;
    std::stringstream full_stack_builder;
    std::stringstream brief_stack_builder;
    std::string last_so_name;
    int index = 0;
    auto _callback = [&](wechat_backtrace::FrameDetail it) {
        std::string so_name = it.map_name;

        char *demangled_name = nullptr;
        int status = 0;

        demangled_name = abi::__cxa_demangle(it.function_name, nullptr, 0, &status);

        if (strstr(it.map_name, "libmatrix-traffic.so") || strstr(it.map_name, "libwechatbacktrace.so")) {
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


static char* getNativeBacktrace() {
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


int (*original_connect)(int fd, const struct sockaddr* addr, socklen_t addr_length);
int my_connect(int fd, sockaddr *addr, socklen_t addr_length) {
    TrafficCollector::enQueueConnect(fd, addr, addr_length);
    return original_connect(fd, addr, addr_length);
}

ssize_t (*original_read)(int fd, void *buf, size_t count);
ssize_t my_read(int fd, void *buf, size_t count) {
    ssize_t ret = original_read(fd, buf, count);
    TrafficCollector::enQueueRx(MSG_TYPE_READ, fd, ret);
    return ret;
}


ssize_t (*original_recv)(int sockfd, void *buf, size_t len, int flags);
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags) {
    ssize_t ret = original_recv(sockfd, buf, len, flags);
    TrafficCollector::enQueueRx(MSG_TYPE_RECV, sockfd, ret);
    return ret;
}


ssize_t (*original_recvfrom)(int sockfd, void *buf, size_t len, int flags,
                             struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t my_recvfrom(int sockfd, void *buf, size_t len, int flags,
                    struct sockaddr *src_addr, socklen_t *addrlen) {
    ssize_t ret = original_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    TrafficCollector::enQueueRx(MSG_TYPE_RECVFROM, sockfd, ret);
    return ret;
}


ssize_t (*original_recvmsg)(int sockfd, struct msghdr *msg, int flags);
ssize_t my_recvmsg(int sockfd, struct msghdr *msg, int flags) {
    ssize_t ret = original_recvmsg(sockfd, msg, flags);
    TrafficCollector::enQueueRx(MSG_TYPE_RECVMSG, sockfd, ret);
    return ret;
}


ssize_t (*original_write)(int fd, const void *buf, size_t count);
ssize_t my_write(int fd, const void *buf, size_t count) {
    ssize_t ret = original_write(fd, buf, count);
    TrafficCollector::enQueueTx(MSG_TYPE_WRITE, fd, ret);
    return ret;
}

ssize_t (*original_send)(int sockfd, const void *buf, size_t len, int flags);
ssize_t my_send(int sockfd, const void *buf, size_t len, int flags) {
    ssize_t ret = original_send(sockfd, buf, len, flags);
    TrafficCollector::enQueueTx(MSG_TYPE_SEND, sockfd, ret);
    return ret;
}

ssize_t (*original_sendto)(int sockfd, const void *buf, size_t len, int flags,
                           const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t my_sendto(int sockfd, const void *buf, size_t len, int flags,
                  const struct sockaddr *dest_addr, socklen_t addrlen) {
    ssize_t ret = original_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    TrafficCollector::enQueueTx(MSG_TYPE_SENDTO, sockfd, ret);
    return ret;
}

ssize_t (*original_sendmsg)(int sockfd, const struct msghdr *msg, int flags);
ssize_t my_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    ssize_t ret = original_sendmsg(sockfd, msg, flags);
    TrafficCollector::enQueueTx(MSG_TYPE_SENDMSG, sockfd, ret);
    return ret;
}

int (*original_close)(int fd);
int my_close(int fd) {
    TrafficCollector::enQueueClose(fd);
    return original_close(fd);
}

static jobject nativeGetTrafficInfoMap(JNIEnv *env, jclass, jint type) {
    return TrafficCollector::getTrafficInfoMap(type);
}

static void nativeReleaseMatrixTraffic(JNIEnv *env, jclass) {
    TrafficCollector::stopLoop();
    TrafficCollector::clearTrafficInfo();
}


static void nativeClearTrafficInfo(JNIEnv *env, jclass) {
    TrafficCollector::clearTrafficInfo();
}

void setStackTrace(char* threadName) {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return;

    jstring jThreadName = env->NewStringUTF(threadName);
    jstring nativeBacktrace;
    if (sDumpNativeBackTrace) {
        nativeBacktrace = env->NewStringUTF(getNativeBacktrace());
    } else {
        nativeBacktrace = env->NewStringUTF("");
    }

    env->CallStaticVoidMethod(gJ.TrafficPlugin, gJ.TrafficPlugin_setFdStackTrace, jThreadName, nativeBacktrace);
    env->DeleteLocalRef(jThreadName);
    env->DeleteLocalRef(nativeBacktrace);
}

static void hookSocket(bool rxHook, bool txHook) {
    if (HOOKED) {
        return;
    }

    xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "connect",
                           (void *) my_connect, (void **) (&original_connect));

    xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "close",
                           (void *) my_close, (void **) (&original_close));

    if (rxHook) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "read",
                               (void *) my_read, (void **) (&original_read));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "recv",
                               (void *) my_recv, (void **) (&original_recv));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "recvfrom",
                               (void *) my_recvfrom, (void **) (&original_recvfrom));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "recvmsg",
                               (void *) my_recvmsg, (void **) (&original_recvmsg));
    }

    if (txHook) {
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "write",
                               (void *) my_write, (void **) (&original_write));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "send",
                               (void *) my_send, (void **) (&original_send));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "sendto",
                               (void *) my_sendto, (void **) (&original_sendto));
        xhook_grouped_register(HOOK_REQUEST_GROUPID_TRAFFIC, ".*\\.so$", "sendmsg",
                               (void *) my_sendmsg, (void **) (&original_sendmsg));
    }

    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libinput\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libgui\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libsensor\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libutils\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libcutils\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libtrace-canary\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libadbconnection_client\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libadbconnection\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libandroid_runtime\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libnetd_client\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libstatssocket\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libc\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libprofile\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libbinder\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libGLES_mali\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libhwui\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libEGL\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libsqlite\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libbase\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*libartbase\\.so$", nullptr);
    xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ".*liblog\\.so$", nullptr);

    xhook_refresh(true);
    HOOKED = true;
}

static void ignoreSo(JNIEnv *env, jobjectArray ignoreSoFiles) {
    for (int i=0; i < env->GetArrayLength(ignoreSoFiles); i++) {
        auto ignoreSoFile = (jstring) (env->GetObjectArrayElement(ignoreSoFiles, i));
        const char *ignoreSoFileChars = env->GetStringUTFChars(ignoreSoFile, 0);
        xhook_grouped_ignore(HOOK_REQUEST_GROUPID_TRAFFIC, ignoreSoFileChars, nullptr);
    }
}

static void nativeInitMatrixTraffic(JNIEnv *env, jclass, jboolean rxEnable, jboolean txEnable, jboolean dumpStackTrace, jboolean dumpNativeBackTrace, jboolean lookupIpAddress, jobjectArray ignoreSoFiles) {
    TrafficCollector::startLoop(dumpStackTrace == JNI_TRUE, lookupIpAddress == JNI_TRUE);
    sDumpNativeBackTrace = (dumpNativeBackTrace == JNI_TRUE);
    ignoreSo(env, ignoreSoFiles);
    hookSocket(rxEnable == JNI_TRUE, txEnable == JNI_TRUE);
}

template <typename T, std::size_t sz>
static inline constexpr std::size_t NELEM(const T(&)[sz]) { return sz; }

static const JNINativeMethod TRAFFIC_METHODS[] = {
        {"nativeInitMatrixTraffic", "(ZZZZZ[Ljava/lang/String;)V", (void *) nativeInitMatrixTraffic},
        {"nativeGetTrafficInfoMap", "(I)Ljava/util/HashMap;", (void *) nativeGetTrafficInfoMap},
        {"nativeClearTrafficInfo", "()V", (void *) nativeClearTrafficInfo},
        {"nativeReleaseMatrixTraffic", "()V", (void *) nativeReleaseMatrixTraffic},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JniInvocation::init(vm);
    JNIEnv *env;

    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
        return -1;

    jclass trafficCollectorCls = env->FindClass("com/tencent/matrix/traffic/TrafficPlugin");
    if (!trafficCollectorCls)
        return -1;
    gJ.TrafficPlugin = static_cast<jclass>(env->NewGlobalRef(trafficCollectorCls));
    gJ.TrafficPlugin_setFdStackTrace =
            env->GetStaticMethodID(trafficCollectorCls, "setStackTrace", "(Ljava/lang/String;Ljava/lang/String;)V");

    if (env->RegisterNatives(
            trafficCollectorCls, TRAFFIC_METHODS, static_cast<jint>(NELEM(TRAFFIC_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(trafficCollectorCls);
    return JNI_VERSION_1_6;
}   // namespace MatrixTraffic
