#ifndef EH_UNWIND_H
#define EH_UNWIND_H
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <ucontext.h>
#include <unwind.h>

typedef struct {
    uintptr_t *frames;
    size_t frames_max_size;
    size_t frames_sz;
    uintptr_t prev_sp;
} eh_info_t;

static _Unwind_Reason_Code eh_unwind_cb(struct _Unwind_Context *unw_ctx, void *arg) {
    uintptr_t pc = _Unwind_GetIP(unw_ctx);
    uintptr_t sp = _Unwind_GetCFA(unw_ctx);

    eh_info_t *info = (eh_info_t *) arg;

    if (info->frames_sz > 0 && pc == info->frames[info->frames_sz - 1] && sp == info->prev_sp)
        return _URC_END_OF_STACK;

    info->frames[info->frames_sz++] = pc;

    if (info->frames_sz >= info->frames_max_size)
        return _URC_END_OF_STACK;

    info->prev_sp = sp;
    return _URC_NO_REASON;
}

size_t eh_unwind(uintptr_t *frames, size_t frames_max) {

    eh_info_t info;
    info.frames = frames;
    info.frames_max_size = frames_max;
    info.frames_sz = 0;
    info.prev_sp = 0;

    _Unwind_Backtrace(eh_unwind_cb, &info);

    return info.frames_sz;
}
#endif // EH_UNWIND_H