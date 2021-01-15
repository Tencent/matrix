package com.tencent.components.backtrace;

public interface WarmUpReporter {

    enum ReportEvent {
        WarmedUp,
        CleanedUp,
        WarmUpThreadBlocked,
        WarmUpFailed,
    }

    void onReport(ReportEvent type, Object ... args);

}
