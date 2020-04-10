//
// Created by Yves on 2020-04-02.
//

#ifndef LIBWXPERF_JNI_ELFINTERFACEARM64_H
#define LIBWXPERF_JNI_ELFINTERFACEARM64_H

#include <unwindstack/RegsArm64.h>
#include <unwindstack/EnhanceDlsym.h>

#define LOGD(TAG, FMT, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#define TAG "Unwind-debug"

namespace unwindstack {

    using namespace std;

    class FallbackPCRange;

    class ElfInterfaceArm64 : public ElfInterface64 {
    public:
        ElfInterfaceArm64(Memory *memory) : ElfInterface64(memory) {
//            EnhanceDlsym::getInstance()->dlopen("/system/lib/libart.so", 0);
        }

        bool PreStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                     bool *finished);

        bool PostStep(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                      bool *finished);

        bool Step(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                  bool *finished) override;

        bool StepPrologue(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                          bool *finished);

    private:
        vector<RegsArm64> prologueFrames;
        uint64_t          stackLimitMax;
        uint64_t          stackLimitMin;
    };

//    mutex mMutex;

    class FallbackPCRange {
    public:

        static FallbackPCRange *GetInstance() {
            if (!INSTANCE) {
                lock_guard<mutex> lock(mMutex);
                if (!INSTANCE) {
                    INSTANCE = new FallbackPCRange;
                }
            }

//            static FallbackPCRange INSTANCE;
            return INSTANCE;
        }

        bool ShouldFallback(uintptr_t pc) {
            LOGD(TAG, "checking %lu", pc);

            for (const auto &range : mSkipFunctions) {
                if (range.first < pc && pc < range.second) {
                    return true;
                }
            }
            return false;
        }

    private:

        FallbackPCRange() {
            void *trampoline;
            if (nullptr !=
                (trampoline = EnhanceDlsym::getInstance()->dlsym(
                        "/apex/com.android.runtime/lib64/libart.so",
                        "art_quick_generic_jni_trampoline"))) {

                mSkipFunctions.push_back({reinterpret_cast<uintptr_t>(trampoline),
                                          reinterpret_cast<uintptr_t>(trampoline) + 0x27C});
            }

            LOGD(TAG, "trampoline %p", trampoline);

            void *art_quick_invoke_static_stub;
            if (nullptr != (art_quick_invoke_static_stub = EnhanceDlsym::getInstance()->dlsym(
                    "/apex/com.android.runtime/lib64/libart.so", "art_quick_invoke_static_stub"))) {
                mSkipFunctions.push_back(
                        {reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub),
                         reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub) + 0x280});
            }

            LOGD(TAG, "art_quick_invoke_static_stub %p", art_quick_invoke_static_stub);

            void *jvalue;
            if (nullptr != (jvalue = EnhanceDlsym::getInstance()->dlsym(
                    "/apex/com.android.runtime/lib64/libart.so",
                    "_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc"))) {
                mSkipFunctions.push_back({reinterpret_cast<uintptr_t>(jvalue),
                                          reinterpret_cast<uintptr_t>(jvalue) + 0x228});
            }

            LOGD(TAG, "jvalue %p", jvalue);
        };

        ~FallbackPCRange() {};

        FallbackPCRange(const FallbackPCRange &);

        FallbackPCRange &operator=(const FallbackPCRange &);

        vector<pair<uintptr_t, uintptr_t>> mSkipFunctions;

        static FallbackPCRange *INSTANCE;

        static mutex mMutex;
    };
}

#endif //LIBWXPERF_JNI_ELFINTERFACEARM64_H
