package com.tencent.matrix.trace.tracer;

import com.tencent.matrix.listeners.IAppForeground;

public interface ITracer extends IAppForeground {

    boolean isAlive();

    void onStartTrace();

    void onCloseTrace();

}
