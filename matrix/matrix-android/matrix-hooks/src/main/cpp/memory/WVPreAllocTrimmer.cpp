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

//
// Created by YinSheng Tang on 2021/5/12.
//

#include <xhook_ext.h>
#include <mutex>
#include <sys/mman.h>
#include <cinttypes>
#include <common/Log.h>
#include <common/ScopedCleaner.h>
#include <common/Macros.h>
#include <common/Maps.h>
#include "WVPreAllocTrimmer.h"

#define LOG_TAG "Matrix.WVPreAllocTrimmer"

enum {
   /**
   * When set, the `reserved_addr` and `reserved_size` fields must point to an
   * already-reserved region of address space which will be used to load the
   * library if it fits.
   *
   * If the reserved region is not large enough, loading will fail.
   */
    ANDROID_DLEXT_RESERVED_ADDRESS      = 0x1,

    /**
     * Like `ANDROID_DLEXT_RESERVED_ADDRESS`, but if the reserved region is not large enough,
     * the linker will choose an available address instead.
     */
    ANDROID_DLEXT_RESERVED_ADDRESS_HINT = 0x2,
};

typedef struct {
    uint64_t flags;
    void* reserved_addr;
    size_t reserved_size;
    int relro_fd;
    int library_fd;
    off64_t library_fd_offset;
    struct android_namespace_t* library_namespace;
} android_dlextinfo;

static const constexpr int LIBLOAD_SUCCESS = 0;
static const constexpr int LIBLOAD_FAILURE = 1;

namespace matrix {
    using namespace wv_prealloc_trimmer;

    static void* (*sOriginalAndroidDlOpenExt)(const char*, int, void*);
    static std::recursive_mutex sDlOpenLock;
    static std::atomic_bool sDlOpenLockGate(false);

    static const constexpr char* PROBE_REQ_TAG = "~~~MEMMISc_Pr0b3_7a9_00l~";
    static const constexpr char* FAKE_RELRO_PATH = "/dev/null";
    static const constexpr uint64_t PROBE_RESP_VALUE = 0xFFFBFFFCFFFDFFFELL;
    static void* sProbedReservedSpaceStart = nullptr;
    static size_t sProbedReservedSpaceSize = 0;
    static std::atomic_bool sInstalled(false);

    template<class TMutex>
    class GateMutexGuard {
    public:
        GateMutexGuard(TMutex& mutex, std::atomic_bool& gate, bool change_gate)
            : mMutex(mutex), mGate(gate), mChangeGate(change_gate) {
            if (change_gate) {
                mMutex.lock();
                mGate.store(true);
            } else if (mGate.load()) {
                mMutex.lock();
            }
        }

        ~GateMutexGuard() {
            if (mChangeGate) {
                mGate.store(false);
                mMutex.unlock();
            } else if (mGate.load()) {
                mMutex.unlock();
            }
        }

    private:
        GateMutexGuard(const GateMutexGuard&) = delete;
        GateMutexGuard& operator =(const GateMutexGuard&) = delete;
        GateMutexGuard& operator =(GateMutexGuard&&) = delete;
        void* operator new(size_t) = delete;
        void* operator new[](size_t) = delete;

        TMutex& mMutex;
        std::atomic_bool& mGate;
        bool mChangeGate;
    };

    static void HackDlExtInfoOnDemand(void* extinfo) {
        auto parsedExtInfo = reinterpret_cast<android_dlextinfo*>(extinfo);
        int extFlags = parsedExtInfo->flags;
        if ((extFlags & ANDROID_DLEXT_RESERVED_ADDRESS) != 0) {
            parsedExtInfo->reserved_addr = nullptr;
            parsedExtInfo->reserved_size = 0;
            extFlags = (extFlags & (~ANDROID_DLEXT_RESERVED_ADDRESS)) | ANDROID_DLEXT_RESERVED_ADDRESS_HINT;
            parsedExtInfo->flags = extFlags;
        }
    }

    static void* HandleAndroidDlOpenExt(const char* filepath, int flags, void* extinfo) {
        GateMutexGuard guard(sDlOpenLock, sDlOpenLockGate, false);
        if (strcmp(filepath, PROBE_REQ_TAG) == 0) {
            auto android_extinfo = reinterpret_cast<android_dlextinfo*>(extinfo);
            sProbedReservedSpaceStart = android_extinfo->reserved_addr;
            sProbedReservedSpaceSize = android_extinfo->reserved_size;
            LOGI(LOG_TAG, "android_dlopen_ext received probe tag, found reserved region: [%p, +%" PRIu32 "].",
                    sProbedReservedSpaceStart, sProbedReservedSpaceSize);
            return reinterpret_cast<void*>(PROBE_RESP_VALUE);
        }
        HackDlExtInfoOnDemand(extinfo);
        return sOriginalAndroidDlOpenExt(filepath, flags, extinfo);
    }

    static bool LocateReservedSpaceByParsingMaps(void** start_out, size_t* size_out) {
        bool found = false;
        matrix::IterateMaps([&](uintptr_t start, uintptr_t end, char perms[4], const char* path, void* args) -> bool {
            if (perms[0] != '-' || perms[1] != '-' || perms[2] != '-' || perms[3] != 'p') {
                // Not match '---p'
                return false;
            }
            if (::strcmp(path, "[anon:libwebview reservation]") == 0) {
                *start_out = reinterpret_cast<void*>(start);
                *size_out = static_cast<size_t>(static_cast<uint64_t>(end) - static_cast<uint64_t>(start));
                found = true;
                return true;
            }
            return false;
        });
        return found;
    }

    #define RETURN_ON_COND(cond, ret_value) \
      do { \
        if (UNLIKELY(cond)) { \
          LOGE(LOG_TAG, "Hit cond: %s, return %s directly.", #cond, #ret_value); \
          return (ret_value); \
        } \
      } while (false)

    static jmethodID GetMethodIDByMetaReflect(JNIEnv* env,
            jclass target_clazz, const char* name, const char** arg_type_names, size_t arg_count) {
        RETURN_ON_COND(env->PushLocalFrame(9 + arg_count) != JNI_OK, nullptr);
        auto cleaner = matrix::MakeScopedCleaner([env]() {
            env->PopLocalFrame(nullptr);
        });

        jclass clazzClazz = env->GetObjectClass(target_clazz);
        jmethodID getDeclaredMethod = env->GetMethodID(clazzClazz,
                "getDeclaredMethod", "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;");
        jobject jGetDeclaredMethod = env->ToReflectedMethod(clazzClazz, getDeclaredMethod, false);
        jobjectArray jTargetArgTypes = env->NewObjectArray(arg_count, clazzClazz, nullptr);
        for (uint32_t i = 0; i < arg_count; ++i) {
            jclass argType = env->FindClass(arg_type_names[i]);
            if (argType == nullptr) {
                env->ExceptionClear();
                LOGE(LOG_TAG, "Cannot find arg type: %s", arg_type_names[i]);
                return nullptr;
            }
            env->SetObjectArrayElement(jTargetArgTypes, i, argType);
        }
        jclass objectClazz = env->FindClass("java/lang/Object");
        jobjectArray jInvokeArgs = env->NewObjectArray(2, objectClazz, nullptr);
        jstring jTargetMethodName = env->NewStringUTF(name);
        env->SetObjectArrayElement(jInvokeArgs, 0, jTargetMethodName);
        env->SetObjectArrayElement(jInvokeArgs, 1, jTargetArgTypes);
        jmethodID invoke = env->GetMethodID(env->GetObjectClass(jGetDeclaredMethod),
                "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
        jobject jTargetMethod = env->CallObjectMethod(jGetDeclaredMethod, invoke, target_clazz, jInvokeArgs);
        if (jTargetMethod == nullptr || env->ExceptionCheck()) {
            env->ExceptionClear();
            return nullptr;
        }
        return env->FromReflectedMethod(jTargetMethod);
    }

    static bool LocateReservedSpaceByProbing(JNIEnv* env,
            jint sdk_ver, jobject class_loader, void** start_out, size_t* size_out) {
        if (sdk_ver > 28) {
            LOGE(LOG_TAG, "Unsupported sdk ver: %d, go failure directly.", sdk_ver);
            return false;
        }
        jclass loaderClazz = env->FindClass("android/webkit/WebViewLibraryLoader");
        if (loaderClazz == nullptr) {
            env->ExceptionClear();
            LOGE(LOG_TAG, "Cannot find loader class, try factory class next.");
            loaderClazz = env->FindClass("android/webkit/WebViewFactory");
        }
        if (loaderClazz != nullptr) {
            LOGD(LOG_TAG, "Found loader/factory class.");
        } else {
            env->ExceptionClear();
            LOGE(LOG_TAG, "Cannot find loader/factory class, go failure directly.");
            return false;
        }

        const char* probeMethodName = "nativeLoadWithRelroFile";
        const char* LS = "java/lang/String";
        const char* LC = "java/lang/ClassLoader";
        jstring jProbeTag = env->NewStringUTF(PROBE_REQ_TAG);
        RETURN_ON_COND(jProbeTag == nullptr, false);
        jstring jFakeRelRoPath = env->NewStringUTF(FAKE_RELRO_PATH);
        RETURN_ON_COND(jFakeRelRoPath == nullptr, false);
        jmethodID probeMethodID = nullptr;
        jint probeMethodRet = 0;
        for (int i = 1; probeMethodID == nullptr && i <= 4; ++i) {
            switch (i) {
                case 1: {
                    const char* typeNameList[] = {LS, LS, LC};
                    probeMethodID = GetMethodIDByMetaReflect(env, loaderClazz, probeMethodName,
                            typeNameList, NELEM(typeNameList));
                    if (probeMethodID != nullptr) {
                        probeMethodRet = env->CallStaticIntMethod(loaderClazz, probeMethodID,
                                jProbeTag, jFakeRelRoPath, class_loader);
                        env->ExceptionClear();
                    }
                    break;
                }
                case 2: {
                    const char* typeNameList[] = {LS, LS, LS, LC};
                    probeMethodID = GetMethodIDByMetaReflect(env, loaderClazz, probeMethodName,
                            typeNameList, NELEM(typeNameList));
                    if (probeMethodID != nullptr) {
                        probeMethodRet = env->CallStaticIntMethod(loaderClazz, probeMethodID,
                                jProbeTag, jFakeRelRoPath, jFakeRelRoPath, class_loader);
                        env->ExceptionClear();
                    }
                    break;
                }
                case 3: {
                    const char* typeNameList[] = {LS, LS, LS, LS, LC};
                    probeMethodID = GetMethodIDByMetaReflect(env, loaderClazz, probeMethodName,
                            typeNameList, NELEM(typeNameList));
                    if (probeMethodID != nullptr) {
                        probeMethodRet = env->CallStaticIntMethod(loaderClazz, probeMethodID,
                                jProbeTag, jProbeTag, jFakeRelRoPath, jFakeRelRoPath, class_loader);
                        env->ExceptionClear();
                    }
                    break;
                }
                case 4: {
                    const char* typeNameList[] = {LS, LS, LS, LS};
                    probeMethodID = GetMethodIDByMetaReflect(env, loaderClazz, probeMethodName,
                            typeNameList, NELEM(typeNameList));
                    if (probeMethodID != nullptr) {
                        if (LIKELY(sdk_ver >= 23)) {
                            probeMethodRet = env->CallStaticIntMethod(loaderClazz, probeMethodID,
                                    jProbeTag, jProbeTag, jFakeRelRoPath, jFakeRelRoPath);
                        } else {
                            probeMethodRet = env->CallStaticBooleanMethod(loaderClazz, probeMethodID,
                                    jProbeTag, jProbeTag, jFakeRelRoPath, jFakeRelRoPath)
                                            ? LIBLOAD_SUCCESS : LIBLOAD_FAILURE;
                        }
                        env->ExceptionClear();
                    }
                    break;
                }
                default: {
                    break;
                }
            }
        }

        if (probeMethodID == nullptr) {
            LOGE(LOG_TAG, "Fail to find probe method.");
            return false;
        }

        if (probeMethodRet == LIBLOAD_SUCCESS) {
            *start_out = sProbedReservedSpaceStart;
            *size_out = sProbedReservedSpaceSize;
            return true;
        } else {
            return false;
        }
    }

    static bool UnmapMemRegion(void* start, size_t size) {
        if (::munmap(start, size) == 0) {
            LOGI(LOG_TAG, "Unmap region [%p, +%" PRIu32 "] successfully.", start, size);
            return true;
        } else {
            int errcode = errno;
            LOGE(LOG_TAG, "Fail to unmap region [%p, +%" PRIu32 "], errcode: %d", start, size, errcode);
            return false;
        }
    }

    bool wv_prealloc_trimmer::Install(JNIEnv* env, jint sdk_ver, jobject classloader) {
        if (sInstalled.load()) {
            LOGE(LOG_TAG, "Already installed.");
            return true;
        }

        void* hLib = xhook_elf_open("libwebviewchromium_loader.so");
        if (hLib == nullptr) {
            LOGE(LOG_TAG, "Fail to open libwebviewchromium_loader.so.");
            return false;
        }
        auto hLibCleaner = MakeScopedCleaner([hLib]() {
            if (hLib != nullptr) {
                xhook_elf_close(hLib);
            }
        });

        int ret = xhook_hook_symbol(hLib, "android_dlopen_ext",
                                    reinterpret_cast<void *>(HandleAndroidDlOpenExt),
                                    reinterpret_cast<void **>(&sOriginalAndroidDlOpenExt));
        if (ret != 0) {
            LOGE(LOG_TAG, "Fail to hook android_dlopen_ext, ret: %d", ret);
            return false;
        }

        LOGI(LOG_TAG, "android_dlopen_ext hook done, locate and unmap reserved space next.");
        bool result = false;
        {
            GateMutexGuard guard(sDlOpenLock, sDlOpenLockGate, true);

            void* reservedSpaceStart = nullptr;
            size_t reservedSpaceSize = 0;

            if (LocateReservedSpaceByParsingMaps(&reservedSpaceStart, &reservedSpaceSize)) {
                LOGI(LOG_TAG, "Reserved space located by parsing maps, start: %p, size: %" PRIu32, reservedSpaceStart, reservedSpaceSize);
                result = true;
            }

            if (!result && LocateReservedSpaceByProbing(env,
                    sdk_ver, classloader, &reservedSpaceStart, &reservedSpaceSize)) {
                LOGI(LOG_TAG, "Reserved space located by probing, start: %p, size: %" PRIu32, reservedSpaceStart, reservedSpaceSize);
                result = true;
            }

            if (result) {
                if (!UnmapMemRegion(reservedSpaceStart, reservedSpaceSize)) {
                    result = false;
                }
            } else {
                LOGE(LOG_TAG, "Cannot locate reserved space.");
            }
        }

        if (result) {
            LOGI(LOG_TAG, "Installed successfully.");
            sInstalled.store(true);
        } else {
            LOGE(LOG_TAG, "Failed, unhook android_dlopen_ext.");
            xhook_hook_symbol(hLib, "android_dlopen_ext",
                              reinterpret_cast<void *>(sOriginalAndroidDlOpenExt), nullptr);
        }

        return result;
    }
}