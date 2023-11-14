#include "StackMeta.h"

inline uint64_t stack_meta_hash(wechat_backtrace::Backtrace *backtrace) {
    uint64_t sum = 1;
    if (backtrace == nullptr) {
        return (uint64_t) sum;
    }
    for (size_t i = 0; i != backtrace->frame_size; i++) {
        sum += (backtrace->frames.get())[i].pc;
    }
    return (uint64_t) sum;
}

inline bool stacktrace_compare(wechat_backtrace::Backtrace *a, wechat_backtrace::Backtrace *b) {
    if (a->frame_size != b->frame_size) {
        return false;
    }

    bool same = true;
    for (size_t i = 0; i < a->frame_size; i++) {
        if ((a->frames.get())[i].pc == (a->frames.get())[i].pc) {
            continue;
        } else {
            same = false;
            break;
        }
    }
    return same;
}

bool delete_backtrace(wechat_backtrace::Backtrace *backtrace) {
    uint64_t hash = stack_meta_hash(backtrace);

    if (hash) {
        if (backtrace_map.exist(hash)) {
            stack_meta_t *target_ext = &backtrace_map.find();
            bool same = false;

            stack_meta_t *target;
            while (target_ext) {
                same = stacktrace_compare(backtrace, target_ext->backtrace);
                target = target_ext;
                if (same) break;
                target_ext = static_cast<stack_meta_t *>(target->ext);
            }

            if (same) {
                target->ref_count--;
                if (target->ref_count == 0) {
                    delete target->backtrace;
                    backtrace_map.remove(hash);
                }
                return true;
            } else {
                return false;
            }
        }
    } else {
        return false;
    }
    return false;
}

wechat_backtrace::Backtrace *deduplicate_backtrace(wechat_backtrace::Backtrace *backtrace) {
    if (backtrace == nullptr) {
        return nullptr;
    }
    uint64_t hash = stack_meta_hash(backtrace);

    if (hash) {
        stack_meta_t *stack_meta;

        if (backtrace_map.exist(hash)) {
            stack_meta = &backtrace_map.find();
            bool same = false;

            stack_meta_t *target = stack_meta;
            stack_meta_t *target_ext = stack_meta;
            while (target_ext) {
                same = stacktrace_compare(backtrace, target_ext->backtrace);
                target = target_ext;
                if (same) break;
                target_ext = static_cast<stack_meta_t *>(target->ext);
            }

            if (!same) {
                if (stack_meta->ref_count == 0 && stack_meta->backtrace->frame_size == 0) {
                    target = stack_meta;
                } else {
                    target->ext = calloc(1, sizeof(stack_meta_t));
                    target = static_cast<stack_meta_t *>(target->ext);
                }
            }

            stack_meta = target;
            if (stack_meta->backtrace == nullptr && backtrace->frame_size != 0) {
                stack_meta->backtrace = backtrace;
            } else if (stack_meta->backtrace->frame_size == 0 && backtrace->frame_size != 0) {
                stack_meta->backtrace = backtrace;
            } else {
                delete backtrace;
            }
            stack_meta->ref_count++;
        } else {
            stack_meta = backtrace_map.insert(hash, {0});
            if (backtrace->frame_size != 0) {
                stack_meta->backtrace = backtrace;
            }
            stack_meta->ref_count++;
        }
        return stack_meta->backtrace;
    }
    return nullptr;
}