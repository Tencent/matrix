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

#ifndef _LIBWECHATBACKTRACE_LOCALMAPS_H
#define _LIBWECHATBACKTRACE_LOCALMAPS_H

#include <stdint.h>
#include <memory>
#include <unwindstack/Maps.h>
#include <unwindstack/JitDebug.h>
#include "BacktraceDefine.h"

namespace wechat_backtrace {

    QUT_EXTERN_C void UpdateLocalMaps();

    QUT_EXTERN_C std::shared_ptr<unwindstack::LocalMaps> &GetMapsCache();

    QUT_EXTERN_C
    std::shared_ptr<unwindstack::JitDebug> &GetJitDebug(std::shared_ptr<unwindstack::Memory> &process_memory);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_LOCALMAPS_H
