//
// Created by Carl on 2020-10-29.
//

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
