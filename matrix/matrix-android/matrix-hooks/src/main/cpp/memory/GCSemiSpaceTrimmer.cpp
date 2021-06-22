//
// Created by YinSheng Tang on 2021/6/17.
//

#include <cstdio>
#include <climits>
#include <cctype>
#include <cstring>
#include <pthread.h>
#include <cinttypes>
#include <mutex>
#include <sys/mman.h>
#include <condition_variable>
#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <syscall.h>
#include <common/Maps.h>
#include <common/Log.h>
#include "GCSemiSpaceTrimmer.h"

#define LOG_TAG "Matrix.GCSemiSpaceTrimmer"

constexpr static const char* PROBE_THREAD_NAME = "gc_sst_holder";

static std::atomic_bool sInstalled(false);
static std::atomic_bool sProcessed(false);
static std::atomic_bool sSuccess(false);
static std::mutex sInstallMutex;
static std::mutex sCommMutex;
static std::condition_variable sCommNotifier;
static std::mutex sBlockerMutex;
static std::condition_variable sBlocker;

class SemiSpaceSearcher {
public:
    SemiSpaceSearcher()
        : mStatus(STATUS_OUTSIDE_TARGETS),
          mSpace1Start(0),
          mSpace1End(0),
          mSpace1Size(0),
          mSpace2Start(0),
          mSpace2End(0),
          mSpace2Size(0) {}

    bool operator()(uintptr_t start, uintptr_t end, char perms[4], const char *path, void *args) {
        while (true) {
            switch (mStatus) {
                case STATUS_OUTSIDE_TARGETS: {
                    if (mSpace1Start != 0 && mSpace1End != 0 && mSpace2Start != 0 && mSpace2End != 0) {
                        mStatus = STATUS_ALL_SPACES_FOUND;
                    } else {
                        if (strstr(path, SS_MAIN_SPACE_NAMES[1]) != nullptr
                            || strstr(path, SS_BUMPER_SPACE_NAMES[1]) != nullptr) {
                            mStatus = STATUS_FOUND_FRIST_SPACE_TWO_ENTRY;
                        } else if (strstr(path, SS_MAIN_SPACE_NAMES[0]) != nullptr
                                   || strstr(path, SS_BUMPER_SPACE_NAMES[0]) != nullptr) {
                            mStatus = STATUS_FOUND_FRIST_SPACE_ONE_ENTRY;
                        } else {
                            // Continue searching next entry.
                            return false;
                        }
                    }
                    break;
                }
                case STATUS_FOUND_FRIST_SPACE_ONE_ENTRY: {
                    if (mSpace1Start == 0) {
                        mSpace1Start = start;
                        mSpace1End = end;
                        mStatus = STATUS_INSIDE_SPACE_ONE_REGION;
                        // Continue searching next entry.
                        return false;
                    } else {
                        mStatus = STATUS_DISCONTINOUS_SPACE_FOUND;
                    }
                    break;
                }
                case STATUS_INSIDE_SPACE_ONE_REGION: {
                    if (strstr(path, SS_MAIN_SPACE_NAMES[1]) != nullptr
                        || strstr(path, SS_BUMPER_SPACE_NAMES[1]) != nullptr) {
                        mStatus = STATUS_FOUND_FRIST_SPACE_TWO_ENTRY;
                    } else if (strstr(path, SS_MAIN_SPACE_NAMES[0]) != nullptr
                               || strstr(path, SS_BUMPER_SPACE_NAMES[0]) != nullptr) {
                        if (start == mSpace1End) {
                            mSpace1End = end;
                            // Continue searching next entry.
                            return false;
                        } else {
                            mStatus = STATUS_DISCONTINOUS_SPACE_FOUND;
                        }
                    } else {
                        mStatus = STATUS_OUTSIDE_TARGETS;
                    }
                    break;
                }
                case STATUS_FOUND_FRIST_SPACE_TWO_ENTRY: {
                    if (mSpace2Start == 0) {
                        mSpace2Start = start;
                        mSpace2End = end;
                        mStatus = STATUS_INSIDE_SPACE_TWO_REGION;
                        // Continue searching next entry.
                        return false;
                    } else {
                        mStatus = STATUS_DISCONTINOUS_SPACE_FOUND;
                    }
                    break;
                }
                case STATUS_INSIDE_SPACE_TWO_REGION: {
                    if (strstr(path, SS_MAIN_SPACE_NAMES[1]) != nullptr
                        || strstr(path, SS_BUMPER_SPACE_NAMES[1]) != nullptr) {
                        if (start == mSpace2End) {
                            mSpace2End = end;
                            // Continue searching next entry.
                            return false;
                        } else {
                            mStatus = STATUS_DISCONTINOUS_SPACE_FOUND;
                        }
                    } else if (strstr(path, SS_MAIN_SPACE_NAMES[0]) != nullptr
                               || strstr(path, SS_BUMPER_SPACE_NAMES[0]) != nullptr) {
                        mStatus = STATUS_FOUND_FRIST_SPACE_ONE_ENTRY;
                    } else {
                        mStatus = STATUS_OUTSIDE_TARGETS;
                    }
                    break;
                }
                case STATUS_ALL_SPACES_FOUND: {
                    mSpace1Size = mSpace1End - mSpace1Start;
                    mSpace2Size = mSpace2End - mSpace2Start;
                    // Interrupt searching.
                    return true;
                }
                case STATUS_DISCONTINOUS_SPACE_FOUND: {
                    // Interrupt searching.
                    return true;
                }
                default: {
                    // Should not be here.
                    return false;
                }
            }
        }
        return false;
    }

    uintptr_t getSpace1Start() const {
        return mSpace1Start;
    }

    uintptr_t getSpace1End() const {
        return mSpace1End;
    }

    uintptr_t getSpace1Size() const {
        return mSpace1Size;
    }

    uintptr_t getSpace2Start() const {
        return mSpace2Start;
    }

    uintptr_t getSpace2End() const {
        return mSpace2End;
    }

    uintptr_t getSpace2Size() const {
        return mSpace2Size;
    }

    bool foundAllSpaces() const {
        return (mStatus == STATUS_ALL_SPACES_FOUND);
    }

    void reset() {
        mStatus = STATUS_OUTSIDE_TARGETS;
        mSpace1Start = 0;
        mSpace1End = 0;
        mSpace1Size = 0;
        mSpace2Start = 0;
        mSpace2End = 0;
        mSpace2Size = 0;
    }

private:
    enum {
        STATUS_OUTSIDE_TARGETS = 0x01,
        STATUS_FOUND_FRIST_SPACE_ONE_ENTRY,
        STATUS_INSIDE_SPACE_ONE_REGION,
        STATUS_FOUND_FRIST_SPACE_TWO_ENTRY,
        STATUS_INSIDE_SPACE_TWO_REGION,
        STATUS_DISCONTINOUS_SPACE_FOUND,
        STATUS_ALL_SPACES_FOUND,
    };

    static constexpr const char* SS_MAIN_SPACE_NAMES[2] = {"dalvik-main space", "dalvik-main space 1"};
    static constexpr const char* SS_BUMPER_SPACE_NAMES[2] = {"Bump pointer space 1", "Bump pointer space 2"};

    int mStatus;
    uintptr_t mSpace1Start;
    uintptr_t mSpace1End;
    uintptr_t mSpace1Size;
    uintptr_t mSpace2Start;
    uintptr_t mSpace2End;
    uintptr_t mSpace2Size;
};

bool matrix::gc_ss_trimmer::IsCompatible() {
    SemiSpaceSearcher semiSpaceSearcher;
    matrix::IterateMaps(std::ref(semiSpaceSearcher));
    return semiSpaceSearcher.foundAllSpaces();
}

bool matrix::gc_ss_trimmer::Install(JNIEnv* env) {
    std::lock_guard installLock(sInstallMutex);

    if (sInstalled.load()) {
        LOGE(LOG_TAG, "Already installed.");
        return JNI_TRUE;
    }

    JavaVM* jvm = nullptr;
    env->GetJavaVM(&jvm);
    if (jvm == nullptr) {
        LOGE(LOG_TAG, "Fail to get JavaVM ptr.");
        return false;
    }

    sSuccess.store(false);

    pthread_t hThread = {};
    pthread_create(&hThread, nullptr, [](void* args) -> void* {
        JavaVM *jvm = reinterpret_cast<JavaVM *>(args);
        JNIEnv *env = nullptr;
        do {
            pthread_setname_np(pthread_self(), PROBE_THREAD_NAME);
            JavaVMAttachArgs jvmArgs = {.version = JNI_VERSION_1_6, .name = PROBE_THREAD_NAME, .group = nullptr};
            if (jvm->AttachCurrentThread(&env, &jvmArgs) != JNI_OK || env == nullptr) {
                LOGE(LOG_TAG, "Fail to attach probe thread, skip processing.");
                break;
            }

            SemiSpaceSearcher semiSpaceSearcher;

            matrix::IterateMaps(std::ref(semiSpaceSearcher));
            if (!semiSpaceSearcher.foundAllSpaces()) {
                // Not found,
                LOGE(LOG_TAG, "Cannot find all semi-spaces, skip processing.");
                break;
            }

            // Found target spaces.
            char vmHeapSize[16] = {};
            if (__system_property_get("dalvik.vm.heapsize", vmHeapSize) < 1) {
                LOGE(LOG_TAG, "Fail to get vm heap size, skip processing.");
                break;
            }
            char* hsCh = vmHeapSize + sizeof(vmHeapSize) - 1;
            size_t hsUnit = 1;
            while (hsCh >= vmHeapSize) {
                if (::isdigit(*hsCh)) {
                    break;
                } else if (*hsCh == 'G' || *hsCh == 'g') {
                    hsUnit = 1024 * 1024 * 1024;
                } else if (*hsCh == 'M' || *hsCh == 'm') {
                    hsUnit = 1024 * 1024;
                } else if (*hsCh == 'K' || *hsCh == 'k') {
                    hsUnit = 1024;
                }
                *hsCh = '\0';
                --hsCh;
            }
            if (hsCh < vmHeapSize) {
                LOGE(LOG_TAG, "Illegal vm heap size value, skip processing.");
                break;
            }
            size_t vmHeapSizeNum = ::strtol(vmHeapSize, nullptr, 10) * hsUnit;
            const size_t space1Size = semiSpaceSearcher.getSpace1Size();
            const size_t space2Size = semiSpaceSearcher.getSpace2Size();
            if (space1Size != space2Size || space1Size != vmHeapSizeNum) {
                LOGE(LOG_TAG, "Unexpected space size, expected: %u, actual_space1: %u, actual_space2: %u",
                     vmHeapSizeNum, space1Size, space2Size);
                break;
            }
            jbyteArray leakedArr = env->NewByteArray(1);
            if (leakedArr == nullptr || env->ExceptionCheck()) {
                env->ExceptionClear();
                LOGE(LOG_TAG, "Fail to leak an probe array, skip processing.");
                break;
            }
            void *arrPtr = env->GetPrimitiveArrayCritical(leakedArr, nullptr);
            if (arrPtr == nullptr) {
                LOGE(LOG_TAG, "Fail to get address of probe array, skip processing.");
                break;
            }
            auto arrPtrValue = reinterpret_cast<uintptr_t>(arrPtr);
            const uintptr_t space1Start = semiSpaceSearcher.getSpace1Start();
            const uintptr_t space1End = semiSpaceSearcher.getSpace1End();
            const uintptr_t space2Start = semiSpaceSearcher.getSpace2Start();
            const uintptr_t space2End = semiSpaceSearcher.getSpace2End();
            if (arrPtrValue >= space1Start && arrPtrValue < space1End) {
                if (::munmap(reinterpret_cast<void *>(space2Start), space2Size) == 0) {
                    LOGI(LOG_TAG, "Unmap space2 successfully.");
                    sSuccess.store(true);
                } else {
                    LOGE(LOG_TAG, "Fail to unmap space2.");
                }
            } else if (arrPtrValue >= space2Start && arrPtrValue < space2End) {
                if (::munmap(reinterpret_cast<void *>(space1Start), space1Size) == 0) {
                    LOGI(LOG_TAG, "Unmap space1 successfully.");
                    sSuccess.store(true);
                } else {
                    LOGE(LOG_TAG, "Fail to unmap space1.");
                }
            } else {
                LOGE(LOG_TAG, "Probe array does not locate in any found spaces, skip processing.");
                break;
            }
        } while (false);

        {
            std::unique_lock uniqueCommLock(sCommMutex);
            sProcessed.store(true);
            sCommNotifier.notify_all();
        }

        if (sSuccess.load()) {
            // Give away CPU times by decrease our priority.
            ::syscall(SYS_setpriority, ::gettid(), 19 /* ANDROID_PRIORITY_LOWEST */);
            // Block forever.
            std::unique_lock uniqueBlockerLock(sBlockerMutex);
            sBlocker.wait(uniqueBlockerLock);
        } else {
            // Notice: Do not call DetachCurrentThread on success since it may trigger JNI-Check and throw exception.
            if (env != nullptr) {
                jvm->DetachCurrentThread();
            }
        }

        return nullptr;
    }, jvm);

    std::unique_lock uniqueCommLock(sCommMutex);
    sCommNotifier.wait(uniqueCommLock, []() -> bool { return sProcessed.load(); });

    if (sSuccess.load()) {
        LOGI(LOG_TAG, "Installed success.");
        sInstalled.store(true);
        return JNI_TRUE;
    } else {
        LOGE(LOG_TAG, "Installed failed.");
        sProcessed.store(false);
        return JNI_FALSE;
    }
}