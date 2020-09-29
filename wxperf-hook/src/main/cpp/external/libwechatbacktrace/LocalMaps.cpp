#include <LocalMaps.h>
#include "../../common/Log.h"

#define WECHAT_BACKTRACE_LOCALMAPS_TAG "LocalMaps"

namespace wechat_backtrace {

static std::shared_ptr<unwindstack::LocalMaps> local_maps_;

static std::mutex unwind_mutex;

void UpdateLocalMaps() {

    std::lock_guard lock(unwind_mutex);

    local_maps_.reset(new unwindstack::LocalMaps);

    if (!local_maps_->Parse()) {
        LOGE(WECHAT_BACKTRACE_LOCALMAPS_TAG, "Failed to parse map data.");
        return;
    }

}

std::shared_ptr<unwindstack::LocalMaps> GetMapsCache() {

    if (local_maps_) {
        return local_maps_;
    }

    UpdateLocalMaps();

    return local_maps_;
}


}  // namespace wechat_backtrace

