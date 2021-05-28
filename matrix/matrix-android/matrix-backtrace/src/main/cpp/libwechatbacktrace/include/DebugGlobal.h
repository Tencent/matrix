/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

#ifndef _LIBWECHATBACKTRACE_GLOBAL_H
#define _LIBWECHATBACKTRACE_GLOBAL_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unwindstack/Elf.h>
#include <unwindstack/Memory.h>

// Forward declarations.
namespace wechat_backtrace {
    class Maps;

    class QuickenMapInfo;

    class DebugGlobal {
    public:
        explicit DebugGlobal(std::shared_ptr<unwindstack::Memory> &memory);

        DebugGlobal(std::shared_ptr<unwindstack::Memory> &memory, std::vector<std::string> &search_libs);

        virtual ~DebugGlobal() = default;

        void SetArch(unwindstack::ArchEnum arch);

        unwindstack::ArchEnum arch() { return arch_; }

    protected:
        bool Searchable(const std::string &name);

        void FindAndReadVariable(wechat_backtrace::Maps *maps, const char *variable);

        virtual bool ReadVariableData(uint64_t offset) = 0;

        virtual void ProcessArch() = 0;

        unwindstack::ArchEnum arch_ = unwindstack::ARCH_UNKNOWN;

        std::shared_ptr<unwindstack::Memory> memory_;
        std::vector<std::string> search_libs_;
    };

}  // namespace unwindstack

#endif  // _LIBWECHATBACKTRACE_GLOBAL_H
