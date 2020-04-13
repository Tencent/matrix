//
// Created by Yves on 2020-04-02.
//

#ifndef LIBWXPERF_JNI_ELFINTERFACEARM64_H
#define LIBWXPERF_JNI_ELFINTERFACEARM64_H

#include <unwindstack/RegsArm64.h>
#include <unwindstack/EnhanceDlsym.h>

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
        uintptr_t stackLimitMax;
        uintptr_t stackLimitMin;

//        uintptr_t lastFP;
//        uintptr_t lastSP;
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

        bool ShouldFallback(uintptr_t pc);

    private:

        FallbackPCRange();

        ~FallbackPCRange() {};

        FallbackPCRange(const FallbackPCRange &);

        FallbackPCRange &operator=(const FallbackPCRange &);

        void SkipDexPC();

        vector<pair<uintptr_t, uintptr_t>> mSkipFunctions;

        static FallbackPCRange *INSTANCE;

        static mutex mMutex;
    };
}

#endif //LIBWXPERF_JNI_ELFINTERFACEARM64_H
