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

#ifndef _LIBWECHATBACKTRACE_QUT_STATISTICS_H
#define _LIBWECHATBACKTRACE_QUT_STATISTICS_H

#include "BacktraceDefine.h"

#ifdef QUT_STATISTIC_ENABLE
#define QUT_STATISTIC(Type, Arg1, Arg2) QutStatistic(Type, (uint64_t)Arg1, (uint64_t)Arg2)
#define QUT_STATISTIC_TIPS(Type, Arg1, Arg2) QutStatisticTips(Type, (uint64_t)Arg1, (uint64_t)Arg2)
#else
#define QUT_STATISTIC(Type, Arg1, Arg2)
#define QUT_STATISTIC_TIPS(Type, Arg1, Arg2)
#endif

namespace wechat_backtrace {

    QUT_EXTERN_C_BLOCK

    enum QutStatisticType : uint32_t {
        InstructionOp = 0,
        InstructionOpOverflowR7 = 1,
        InstructionOpOverflowR11 = 2,
        InstructionOpOverflowJNISP = 3,
        InstructionOpOverflowR4 = 4,
        InstructionOpImmNotAligned = 5,

        IgnoreUnsupportedArmExidx = 10,
        IgnoreInstructionOpOverflowX29 = 11,
        IgnoreInstructionOpOverflowX20 = 12,

        InstructionEntriesArmExidx = 20,
        InstructionEntriesEhFrame = 21,
        InstructionEntriesDebugFrame = 22,

        IgnoreUnsupportedDwarfLocation = 30,
        IgnoreUnsupportedDwarfLocationOffset = 31,
        IgnoreUnsupportedCfaDwarfLocationRegister = 32,

        UnsupportedDwarfOp_OpBreg_Reg = 33,
        UnsupportedDwarfOp_OpBregx_Reg = 34,
        UnsupportedDwarfLocationValOffset = 35,
        UnsupportedDwarfLocationRegister = 36,
        UnsupportedDwarfLocationExpression = 37,
        UnsupportedDwarfLocationValExpressionWithoutDexPc = 38,
        UnsupportedCfaDwarfLocationValExpression = 39,

    };

    void SetCurrentStatLib(const std::string);

    void QutStatistic(QutStatisticType, uint64_t, uint64_t);

    void QutStatisticTips(QutStatisticType, uint64_t, uint64_t);

    void DumpQutStatResult(std::vector<uint32_t> &processed_result);

    QUT_EXTERN_C_BLOCK_END
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUT_STATISTICS_H
