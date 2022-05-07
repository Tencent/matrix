//
// Created by tomystang on 2020/10/15.
//

#include <sstream>
#include <optional>
#include <iomanip>
#include <cinttypes>
#include <dlfcn.h>
#include <util/Auxiliary.h>
#include <util/Unwind.h>
#include <util/Log.h>
#include <FastUnwind.h>
#include <unwindstack/LocalUnwinder.h>

using namespace memguard;
using namespace unwindstack;

static std::optional<LocalUnwinder> sLocalUnwinder;

bool memguard::unwind::Initialize() {
    sLocalUnwinder.emplace();
    return sLocalUnwinder->Init();
}

int memguard::unwind::UnwindStack(void** pcs, size_t max_count) {
#if defined(__aarch64__) || defined(__i386__)
    return fastunwind::Unwind(pcs, max_count);
#else
    return sLocalUnwinder->Unwind(pcs, max_count);
#endif
}

int memguard::unwind::UnwindStack(void* ucontext, void** pcs, size_t max_count) {
    return sLocalUnwinder->Unwind(ucontext, pcs, max_count);
}

char* memguard::unwind::GetStackElementDescription(const void* pc, char* desc_out, size_t max_length) {
    std::string desc;
    size_t outputLen = max_length - 1;
    if (sLocalUnwinder->GetStackElementString((uintptr_t) pc, &desc)) {
        if (desc.length() < outputLen) {
            outputLen = desc.length();
        }
        strncpy(desc_out, desc.c_str(), outputLen);
    } else {
        const char* failureStr = "<?\?\?>";
        size_t failureStrLen = strlen(failureStr);
        if (failureStrLen < outputLen) {
            outputLen = failureStrLen;
        }
        strncpy(desc_out, failureStr, outputLen);
    }
    desc_out[outputLen] = '\0';
    return desc_out;
}