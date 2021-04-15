//
// Created by Carl on 2020-10-29.
//

#ifndef _LIBWECHATBACKTRACE_QUT_STATISTICS_H
#define _LIBWECHATBACKTRACE_QUT_STATISTICS_H

//#define QUT_STATISTIC_ENABLE  // TODO

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
        InstructionOpOverflowX29 = 4,
        InstructionOpOverflowR4 = 5,
        InstructionOpOverflowX20 = 6,

        InstructionOpImmNotAligned = 9,
        UnsupportedArmExdix = 10,

        InstructionEntriesArmExidx = 20,
        InstructionEntriesEhFrame = 21,
        InstructionEntriesDebugFrame = 22,

        UnsupportedDwarfLocation = 30,
        UnsupportedDwarfLocationValOffset = 31,
        UnsupportedDwarfLocationRegister = 32,
        UnsupportedDwarfLocationExpression = 33,
        UnsupportedDwarfLocationValExpressionWithoutDexPc = 34,
        UnsupportedDwarfLocationUndefined = 35,
        UnsupportedDwarfLocationOffset = 36,

        UnsupportedCfaDwarfLocationRegister = 37,
        UnsupportedCfaDwarfLocationValExpression = 38,

        UnsupportedDwarfOp_OpBreg_Reg = 41,
        UnsupportedDwarfOp_OpBregx_Reg = 42,

    };

    void SetCurrentStatLib(const std::string);

    void QutStatistic(QutStatisticType, uint64_t, uint64_t);

    void QutStatisticTips(QutStatisticType, uint64_t, uint64_t);

    void DumpQutStatResult();

    QUT_EXTERN_C_BLOCK_END
}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUT_STATISTICS_H
