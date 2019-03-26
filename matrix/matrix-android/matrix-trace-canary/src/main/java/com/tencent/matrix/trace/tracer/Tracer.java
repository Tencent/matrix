package com.tencent.matrix.trace.tracer;

import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.util.MatrixLog;

public abstract class Tracer extends LooperObserver implements ITracer {

    private volatile boolean isAlive = false;
    private static final String TAG = "Matrix.Tracer";

    protected void onAlive() {
        MatrixLog.i(TAG, "onAlive");

    }

    protected void onDead() {
        MatrixLog.i(TAG, "onDead");
    }

    @Override
    final synchronized public void onStartTrace() {
        if (!isAlive) {
            this.isAlive = true;
            onAlive();
        }
    }

    @Override
    final synchronized public void onCloseTrace() {
        if (isAlive) {
            this.isAlive = false;
            onDead();
        }
    }

    @Override
    public boolean isAlive() {
        return isAlive;
    }


}
