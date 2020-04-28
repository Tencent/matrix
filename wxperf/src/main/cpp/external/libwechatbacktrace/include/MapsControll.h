//
// Created by Carl on 2020-04-28.
//

#ifndef LIBWXPERF_JNI_MAPSCONTROLL_H
#define LIBWXPERF_JNI_MAPSCONTROLL_H

#include <type_traits>
#include <unwindstack/Maps.h>

namespace wechat_backtrace {

    unwindstack::LocalMaps * GetMapsCache();

    void UpdateMapsCache(unwindstack::LocalMaps *);

    void UpdateFallbackPCRange();
}

#endif //LIBWXPERF_JNI_MAPSCONTROLL_H
