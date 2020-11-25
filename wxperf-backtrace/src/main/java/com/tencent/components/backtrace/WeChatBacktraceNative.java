package com.tencent.components.backtrace;

import android.support.annotation.Keep;

@Keep
public class WeChatBacktraceNative {

    @Keep
    static native void setPackageName(String packageName);

    @Keep
    static native void setSavingPath(String savingPath);

    @Keep
    static native void setWarmedUp(boolean hasWarmUp);

    @Keep
    static native void consumeRequestedQut();

    @Keep
    static native void warmUp(String sopath);

    @Keep
    static native void statistic(String sopath);

    @Keep
    static void requestQutGenerate() { // Be called from native.
        WeChatBacktrace.instance().requestQutGenerate();
    }

}
