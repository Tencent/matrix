package com.tencent.components.backtrace;

import android.os.Bundle;

public interface WarmUpReporter {

    enum ReportEvent {
        WarmedUp,
        CleanedUp,
        WarmUpThreadBlocked,
    }

    void onReport(ReportEvent type);

}
