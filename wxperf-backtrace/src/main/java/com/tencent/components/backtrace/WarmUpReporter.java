package com.tencent.components.backtrace;

public interface WarmUpReporter {

    enum ReportEvent {
        WarmedUp,
        CleanedUp,
        WarmUpThreadBlocked,
    }

    void onReport(ReportEvent type);

}
