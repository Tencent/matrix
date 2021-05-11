#include <LocalMaps.h>
#include <Log.h>

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    QUT_EXTERN_C_BLOCK

    DEFINE_STATIC_LOCAL(std::shared_ptr<unwindstack::LocalMaps>, local_maps_, );
    DEFINE_STATIC_LOCAL(std::mutex, unwind_mutex, );

    BACKTRACE_EXPORT void UpdateLocalMaps() {

        std::lock_guard lock(unwind_mutex);

        shared_ptr<LocalMaps> new_maps = make_shared<LocalMaps>();
        if (!new_maps->Parse()) {
            QUT_LOG("Failed to parse local map data.");
            return;
        }

        local_maps_ = new_maps;
    }

    BACKTRACE_EXPORT std::shared_ptr<unwindstack::LocalMaps> GetMapsCache() {
        if (!local_maps_) {
            UpdateLocalMaps();
        }
        std::lock_guard lock(unwind_mutex);
        return local_maps_;
    }

    QUT_EXTERN_C_BLOCK_END

}  // namespace wechat_backtrace

