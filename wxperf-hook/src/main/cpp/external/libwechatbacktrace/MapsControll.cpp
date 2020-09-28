//
// Created by Carl on 2020-04-28.
//

#include <type_traits>
#include <cinttypes>
#include <string>
#include <fstream>
#include <fcntl.h>

#include <MapsControll.h>
#include <ElfInterfaceArm64.h>
#include <unwindstack/EnhanceDlsym.h>
#include <FastUnwinder.h>
#include "../../common/Log.h"

namespace wechat_backtrace {

    using namespace std;

    static unique_ptr<unwindstack::LocalMaps> gMapsCache;  // TODO

    static pthread_mutex_t unwind_mutex = PTHREAD_MUTEX_INITIALIZER;

    static inline void update_maps() {

        pthread_mutex_lock(&unwind_mutex);

        unwindstack::LocalMaps *localMaps = gMapsCache.get();
        if (!localMaps) {
            localMaps = new unwindstack::LocalMaps;
        } else {
            delete localMaps;
            localMaps = new unwindstack::LocalMaps;
        }
        if (!localMaps->Parse()) {
            LOGE(TAG, "Failed to parse map data.");
            gMapsCache.reset(nullptr);
            pthread_mutex_unlock(&unwind_mutex);
            return;
        }
        gMapsCache.reset(localMaps);
        pthread_mutex_unlock(&unwind_mutex);
    }

    unwindstack::LocalMaps *GetMapsCache() {
        unwindstack::LocalMaps *maps = gMapsCache.get();

        if (maps == NULL) {
            update_maps();
            return gMapsCache.get();
        }

        return maps;
    }

    void UpdateMapsCache(unwindstack::LocalMaps *maps) {
        gMapsCache.reset(maps);
    }

    static inline bool EndWith(std::string const &value, std::string const &ending) {
        if (ending.size() > value.size()) {
            return false;
        }
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

    static inline void SkipDexPC() {

        uintptr_t map_base_addr;
        uintptr_t map_end_addr;
        char      map_perm[5];

        ifstream    input("/proc/self/maps");
        std::string aline;

        while (getline(input, aline)) {
            if (sscanf(aline.c_str(),
                       "%" PRIxPTR "-%" PRIxPTR " %4s",
                       &map_base_addr,
                       &map_end_addr,
                       map_perm) != 3) {
                continue;
            }

            if ('r' == map_perm[0]
                && 'x' == map_perm[2]
                && 'p' == map_perm[3]
                &&
                (EndWith(aline, ".dex")
                 || EndWith(aline, ".odex")
                 || EndWith(aline, ".vdex"))) {

                GetSkipFunctions()->push_back({map_base_addr, map_end_addr});
            }
        }
    }

    void UpdateFallbackPCRange() {

        GetSkipFunctions()->clear();

        void *handle = unwindstack::enhance::dlopen("libart.so", 0);

        if (handle) {

            void *trampoline =
                         unwindstack::enhance::dlsym(handle, "art_quick_generic_jni_trampoline");

            if (nullptr != trampoline) {
                GetSkipFunctions()->push_back({reinterpret_cast<uintptr_t>(trampoline),
                                               reinterpret_cast<uintptr_t>(trampoline) + 0x27C});
            }

            void *art_quick_invoke_static_stub = unwindstack::enhance::dlsym(
                    handle,
                    "art_quick_invoke_static_stub");
            if (nullptr != art_quick_invoke_static_stub) {
                GetSkipFunctions()->push_back(
                        {reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub),
                         reinterpret_cast<uintptr_t >(art_quick_invoke_static_stub) + 0x280});
            }
        }
        unwindstack::enhance::dlclose(handle);

        SkipDexPC();
    }

}
