/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <LocalMaps.h>
#include <Log.h>
#include <include/unwindstack/JitDebug.h>
#include <MemoryLocal.h>

namespace wechat_backtrace {

    using namespace std;
    using namespace unwindstack;

    QUT_EXTERN_C_BLOCK

    DEFINE_STATIC_LOCAL(std::shared_ptr<unwindstack::LocalMaps>, local_maps_, );
    DEFINE_STATIC_LOCAL(std::shared_ptr<unwindstack::JitDebug>, jit_debug_, );
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

    BACKTRACE_EXPORT
    std::shared_ptr<unwindstack::LocalMaps> &GetMapsCache() {
        if (!local_maps_) {
            UpdateLocalMaps();
        }
        std::lock_guard lock(unwind_mutex);
        return local_maps_;
    }

    BACKTRACE_EXPORT
    std::shared_ptr<unwindstack::JitDebug> &GetJitDebug(shared_ptr<Memory> &process_memory) {
        if (!jit_debug_) {
            std::lock_guard lock(unwind_mutex);
            shared_ptr<JitDebug> jit_debug = std::make_shared<JitDebug>(process_memory);
            jit_debug_ = jit_debug;
        }
        std::lock_guard lock(unwind_mutex);
        return jit_debug_;
    }


    QUT_EXTERN_C_BLOCK_END

}  // namespace wechat_backtrace

