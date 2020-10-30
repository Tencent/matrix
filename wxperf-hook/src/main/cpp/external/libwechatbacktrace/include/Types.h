#ifndef _LIBWECHATBACKTRACE_TYPES_H
#define _LIBWECHATBACKTRACE_TYPES_H

namespace wechat_backtrace {

typedef uintptr_t uptr;
//typedef usize_t usize;

#ifdef __arm__
typedef uint32_t addr_t;
#else
typedef uint64_t addr_t;
#endif
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_TYPES_H
