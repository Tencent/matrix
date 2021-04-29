// Copyright 2020 Tencent Inc.  All rights reserved.
//
// Author: leafjia@tencent.com
//
// SignalHandler.cpp

#include "AnrDumper.h"

#include <dirent.h>
#include <pthread.h>
#include <cxxabi.h>
#include <unistd.h>
#include <syscall.h>
#include <cstdlib>

#include <optional>
#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iosfwd>
#include <string>
#include <fcntl.h>
#include <nativehelper/scoped_utf_chars.h>

#include "DynamicLoader.h"
#include "nativehelper/scoped_local_ref.h"
#include "MatrixTracer.h"
#include "Logging.h"
#include "Support.h"

#define SIGNAL_CATCHER_THREAD_NAME "Signal Catcher"
#define SIGNAL_CATCHER_THREAD_SIGBLK 0x1000
#define O_WRONLY 00000001
#define O_CREAT 00000100
#define O_TRUNC 00001000


#define voidp void*
using std::string;

namespace MatrixTracer {


AnrDumper::AnrDumper(const char* anrTraceFile, const char* printTraceFile, AnrDumper::DumpCallbackFunction &&callback) :
        mAnrTraceFile(anrTraceFile), mPrintTraceFile(printTraceFile), mCallback(callback) {
    // ART blocks SIGQUIT for its internal use. Re-enable it.
    // 必须unblock，否则signal handler无法接收到信号，而是由signal_cahcher线程中的sigwait接收信号，走一般的ANR流程
    sigset_t sigSet;
    sigemptyset(&sigSet);
    sigaddset(&sigSet, SIGQUIT);
    pthread_sigmask(SIG_UNBLOCK, &sigSet, nullptr);
}


//AnrDumper::AnrDumper(char* anrTraceFile, char* printTraceFile, DumpCallbackFunction&& callback) : mCallback(callback) {
//    // ART blocks SIGQUIT for its internal use. Re-enable it.
//    // 必须unblock，否则signal handler无法接收到信号，而是由signal_cahcher线程中的sigwait接收信号，走一般的ANR流程
//    sigset_t sigSet;
//    sigemptyset(&sigSet);
//    sigaddset(&sigSet, SIGQUIT);
//    pthread_sigmask(SIG_UNBLOCK, &sigSet, nullptr);
//}

struct ArtInterface {
    static constexpr const char *kLibcxxCerrName = "_ZNSt3__14cerrE";
    static constexpr const char *kLibartRuntimeInstanceName = "_ZN3art7Runtime9instance_E";
    static constexpr const char *kLibartRuntimeDumpName =
            "_ZN3art7Runtime14DumpForSigQuitERNSt3__113basic_ostreamIcNS1_11char_traitsIcEEEE";

    void *libcxxCerr;
    void **libartRuntimeInstance;
    void (*libartRuntimeDump)(void *, void *);

    ArtInterface() :
            libcxxCerr(nullptr),
            libartRuntimeInstance(nullptr),
            libartRuntimeDump(nullptr) {
        DynamicLoader libartSo("libart.so");
        if (libartSo) {
            libartRuntimeInstance = static_cast<void **>(
                    libartSo.findSymbol(kLibartRuntimeInstanceName));
            libartRuntimeDump = reinterpret_cast<void (*)(void *, void *)>(
                    libartSo.findSymbol(kLibartRuntimeDumpName));
        } else {
            ALOGE("Cannot find libart.so");
        }

        const std::string& artPath = libartSo.fullPath();
        if (Support::stringEndsWith(artPath, "libart.so")) {
            std::string cxxPath(artPath);
            constexpr std::string_view sv("libart.so");
            cxxPath.replace(cxxPath.size() - sv.size(), sv.size(), "libc++.so");

            libcxxCerr = DynamicLoader(cxxPath.c_str()).findSymbol(kLibcxxCerrName);
        }

        if (!libcxxCerr) {
            DynamicLoader libcxxSo("libc++.so");
            if (libcxxSo) {
                libcxxCerr = libcxxSo.findSymbol(kLibcxxCerrName);
            } else {
                ALOGE("Cannot find libc++.so");
            }
        }
    }

    bool valid() const { return libcxxCerr && libartRuntimeInstance && libartRuntimeDump; }
};
std::optional<const ArtInterface> sArtInterface(std::nullopt);

static int getSignalCatcherThreadId() {
    char taskDirPath[128];
    DIR *taskDir;
    uint64_t sigblk;
    uint32_t signalCatcherTid = -1;

    snprintf(taskDirPath, sizeof(taskDirPath), "/proc/%d/task", getpid());
    if ((taskDir = opendir(taskDirPath)) == nullptr) return -1;
    struct dirent *dent;
    pid_t tid;
    while ((dent = readdir(taskDir)) != nullptr) {
        tid = atoi(dent->d_name);
        if (tid <= 0) continue;

        char threadName[1024];
        char commFilePath[1024];
        snprintf(commFilePath, sizeof(commFilePath), "/proc/%d/task/%d/comm", getpid(), tid);

        Support::readFileAsString(commFilePath, threadName, sizeof(threadName));

        if(strncmp(SIGNAL_CATCHER_THREAD_NAME, threadName , sizeof(SIGNAL_CATCHER_THREAD_NAME)-1) != 0) {
            continue;
        }

        sigblk = 0;
        char taskPath[128];
        snprintf(taskPath, sizeof(taskPath), "/proc/%d/status", tid);

        ScopedFileDescriptor fd(open(taskPath, O_RDONLY, 0));
        LineReader lr(fd.get());
        const char *line;
        size_t len;
        while (lr.getNextLine(&line, &len)) {
            if (1 == sscanf(line, "SigBlk: %" SCNx64, &sigblk)) break;
            lr.popLine(len);
        }
;
        if (SIGNAL_CATCHER_THREAD_SIGBLK != sigblk) continue;
        signalCatcherTid = tid;
        break;
    }
    closedir(taskDir);
    return signalCatcherTid;
}

static void *anr_callback(void* args) {
    anrDumpCallback(0, "");
    return nullptr;
}

SignalHandler::Result AnrDumper::handleSignal(int sig, const siginfo_t *, void *uc) {
    // Only process SIGQUIT, which indicates an ANR.
    if (sig != SIGQUIT) return NOT_HANDLED;
    // Call dumper in separated thread.
    pthread_t thd;
    pthread_create(&thd, nullptr, anr_callback, nullptr);
    pthread_join(thd,nullptr);

    doDump(true);

    int tid = getSignalCatcherThreadId();
    syscall(SYS_tgkill, getpid(), tid, SIGQUIT);
    return HANDLED_NO_RETRIGGER;

}

static void *anr_trace_callback(void* args) {
    anrDumpTraceCallback();
    return nullptr;
}

static void *print_trace_callback(void* args) {
    printTraceCallback();
    return nullptr;
}




int AnrDumper::doDump(bool isAnr) {

    const char* dumpFile;

    if (isAnr && strlen(mAnrTraceFile) == 0) {
        return EINVAL;
    }

    if(!isAnr && strlen(mPrintTraceFile) == 0) {
        return EINVAL;
    }

    if(isAnr) {
        dumpFile = mAnrTraceFile;
    } else {
        dumpFile = mPrintTraceFile;
    }

    // Initialize ART interface, if not initialized yet. SignalHandler ensures that
    // handlers are called serialized, so it's safe to be done without synchronization.
    if (!sArtInterface) {
        sArtInterface.emplace();
        if (!sArtInterface->valid()) {
            ALOGE("Cannot load need symbols. [%p, %p, %p]",
                  sArtInterface->libcxxCerr,
                  sArtInterface->libartRuntimeInstance,
                  sArtInterface->libartRuntimeDump);

            sArtInterface.reset();
            return ELIBBAD;
        }
    }

    // Open log file for writing.
    ScopedFileDescriptor dumpFd(open(dumpFile, O_WRONLY | O_CREAT | O_TRUNC, 0600));
    if (!dumpFd.valid()) {
        return errno;
    }

    // Save stderr descriptor for later recovery.
    ScopedFileDescriptor savedStderr(dup(STDERR_FILENO));
    if (!savedStderr.valid()) {
        ALOGI("dup(stderr) failed, errno: %d", errno);
        return errno;
    }

    // dup dump file to stderr.
    if (dup2(dumpFd.get(), STDERR_FILENO) != STDERR_FILENO) {
        ALOGI("dup2(dump, stderr) failed, errno: %d", errno);
        return errno;
    }

    // Magically call Runtime::dumpForSigQuit in libart, specifying std::ostream object std::cerr
    // as its destination. This will make ART to print dump info to stderr, which was replaced
    // with our dump file.
    try {
        void *runtime = *(sArtInterface->libartRuntimeInstance);
        sArtInterface->libartRuntimeDump(runtime, sArtInterface->libcxxCerr);
    } catch (std::exception& e) {
    }

    // Recover stderr.
    dup2(savedStderr.get(), STDERR_FILENO);

    pthread_t thd;
    if (isAnr) {
        pthread_create(&thd, nullptr, anr_trace_callback, nullptr);
    } else {
        pthread_create(&thd, nullptr, print_trace_callback, nullptr);
    }
    pthread_detach(thd);
    return 0;
}


}   // namespace MatrixTracer
