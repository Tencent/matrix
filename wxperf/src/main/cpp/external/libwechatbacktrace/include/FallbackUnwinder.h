#ifndef _LIBUNWINDSTACK_FALLBCKUNWINDER_H
#define _LIBUNWINDSTACK_FALLBCKUNWINDER_H

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include <unwindstack/DexFiles.h>
#include <unwindstack/Error.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/Unwinder.h>

namespace unwindstack {

class FallbackUnwinder : public Unwinder {

public:
    FallbackUnwinder(Maps *maps, Regs *regs,std::shared_ptr<Memory> processMemory)
            : Unwinder(0, maps, regs, processMemory) {}

    virtual ~FallbackUnwinder() = default;

    void fallbackUnwindFrame(bool &finished);
    void fallbackUnwindFrameImpl(bool &finished, bool return_address_attempt);

    Regs* getRegs() { return regs_; };
};

}  // namespace unwindstack

#endif  // _LIBUNWINDSTACK_FALLBCKUNWINDER_H
