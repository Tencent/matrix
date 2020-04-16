//
// Created by Yves on 2020-04-02.
//

#ifndef LIBWXPERF_JNI_ELFINTERFACEARM64_H
#define LIBWXPERF_JNI_ELFINTERFACEARM64_H

#include <unwindstack/RegsArm64.h>
#include <unwindstack/EnhanceDlsym.h>
#include "TimeUtil.h"

#define LOGD(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##args)
#define LOGI(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##args)
#define LOGE(TAG, FMT, args...) //__android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##args)

#define TAG "Unwind-debug"

namespace unwindstack {

    using namespace std;

    class FallbackPCRange;

    class ElfInterfaceArm64 : public ElfInterface64 {
    public:
        ElfInterfaceArm64(Memory *memory) : ElfInterface64(memory) {
//            EnhanceDlsym::getInstance()->dlopen("/system/lib/libart.so", 0);

        }

        bool Step(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                  bool *finished) override;

        inline bool StepPrologue(uint64_t pc, uint64_t load_bias, Regs *regs, Memory *process_memory,
                          bool *finished);

    private:
//        pthread_attr_t attr;

    };

//    extern mutex mMutex;

    extern pthread_mutex_t pthread_mutex;

    class FallbackPCRange {
    public:

        static FallbackPCRange *GetInstance() {
            if (!INSTANCE) {
//                lock_guard<mutex> lock(mMutex);
                pthread_mutex_lock(&pthread_mutex);
                if (!INSTANCE) {
                    INSTANCE = new FallbackPCRange;
                }
                pthread_mutex_unlock(&pthread_mutex);
            }

            return INSTANCE;
        }

        inline bool ShouldPCFallback(uintptr_t pc);

        inline bool ShouldFPSPFallback(uintptr_t fp, uintptr_t sp, RegsArm64* regs);

    private:

        FallbackPCRange();

        ~FallbackPCRange() {};

        FallbackPCRange(const FallbackPCRange &);

        FallbackPCRange &operator=(const FallbackPCRange &);

        void SkipDexPC();

        vector<pair<uintptr_t, uintptr_t>> mSkipFunctions;

        int fd;

        static FallbackPCRange *INSTANCE;

//        static mutex mMutex;
    };
}

#endif //LIBWXPERF_JNI_ELFINTERFACEARM64_H
