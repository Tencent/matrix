#ifndef _LIBWECHATBACKTRACE_LOCALMAPS_H
#define _LIBWECHATBACKTRACE_LOCALMAPS_H

#include <stdint.h>
#include <memory>
#include <unwindstack/Maps.h>
#include "Predefined.h"

namespace wechat_backtrace {

    QUT_EXTERN_C void UpdateLocalMaps();

    QUT_EXTERN_C std::shared_ptr<unwindstack::LocalMaps> GetMapsCache();

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_LOCALMAPS_H
