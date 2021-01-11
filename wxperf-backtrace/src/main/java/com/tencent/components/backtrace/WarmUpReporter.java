package com.tencent.components.backtrace;

public interface WarmUpReporter {

    void onWarmedUp(boolean warmedUp);

    void onCleanedUp(boolean cleanedUp);

    void onWarmUpThreadBlocked(boolean blocked);

}
