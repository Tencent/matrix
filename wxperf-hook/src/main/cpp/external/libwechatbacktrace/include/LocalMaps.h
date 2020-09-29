#ifndef _LIBWECHATBACKTRACE_LOCALMAPS_H
#define _LIBWECHATBACKTRACE_LOCALMAPS_H

#include <stdint.h>
#include <memory>
#include <unwindstack/Maps.h>

namespace wechat_backtrace {

    void UpdateLocalMaps();

    std::shared_ptr<unwindstack::LocalMaps> GetMapsCache();

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_LOCALMAPS_H
