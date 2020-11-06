//
// Created by Carl on 2020-10-29.
//

#ifndef _LIBWECHATBACKTRACE_QUT_STATISTICS_H
#define _LIBWECHATBACKTRACE_QUT_STATISTICS_H

namespace wechat_backtrace {

//#define QUT_STATISTIC_ENABLE

#ifdef QUT_STATISTIC_ENABLE
#define QUT_STATISTIC(Type, Arg1, Arg2) GetQutStatistic()(Type, (uint64_t)Arg1, (uint64_t)Arg2)
#else
#define QUT_STATISTIC(Type, Arg1, Arg2)
#endif

enum QutStatisticType : uint32_t {
    InstructionOp = 0,
    InstructionOpOverflow = 1,
    InstructionOpImmNotAligned = 2,
    UnsupportedArmExdix = 10,

    UnsupportedDwarfOp = 20,
    UnsupportedDwarfOp_OpBreg_Reg = 21,
    UnsupportedDwarfOp_OpBregx_Reg = 22,

    UnsupportedDwarfLocation = 30,
    UnsupportedDwarfLocationValOffset = 31,
    UnsupportedDwarfLocationRegister = 32,
    UnsupportedDwarfLocationExpression = 33,
    UnsupportedDwarfLocationValExpressionWithoutDexPc = 34,
    UnsupportedDwarfLocationUndefined = 35,
    UnsupportedDwarfLocationOffset = 36,

    UnsupportedCfaDwarfLocationRegister = 37,
    UnsupportedCfaDwarfLocationValExpression = 38,
};

typedef void(*QutStatisticInstruction)(QutStatisticType, uint64_t, uint64_t);

QutStatisticInstruction GetQutStatistic();

void SetQutStatistic(QutStatisticInstruction statistic_func);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUT_STATISTICS_H
