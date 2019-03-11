package com.tencent.matrix.trace.tracer;

public interface ITracer {

    boolean isAlive();

    void onStartTrace();

    void onCloseTrace();

}
