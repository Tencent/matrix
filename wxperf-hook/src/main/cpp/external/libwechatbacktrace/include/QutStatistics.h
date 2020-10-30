//
// Created by Carl on 2020-10-29.
//

#ifndef _LIBWECHATBACKTRACE_QUT_STATISTICS_H
#define _LIBWECHATBACKTRACE_QUT_STATISTICS_H

namespace wechat_backtrace {

enum QutStatisticType : uint32_t {
    InstructionOp = 0,
    UnsupportedArmExdix,
};

typedef void(*QutStatisticInstruction)(QutStatisticType, uint32_t, uint32_t);

QutStatisticInstruction GetQutStatistic();

void SetQutStatistic(QutStatisticInstruction statistic_func);

}  // namespace wechat_backtrace

#endif  // _LIBWECHATBACKTRACE_QUT_STATISTICS_H
