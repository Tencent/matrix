package com.tencent.matrix.trace.tracer;

import android.support.annotation.CallSuper;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.util.MatrixLog;

public abstract class Tracer extends LooperObserver implements ITracer {

    private volatile boolean isAlive = false;
    private static final String TAG = "Matrix.Tracer";

    @CallSuper
    protected void onAlive() {
        MatrixLog.i(TAG, "[onAlive] %s", this.getClass().getName());

    }

    @CallSuper
    protected void onDead() {
        MatrixLog.i(TAG, "[onDead] %s", this.getClass().getName());
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
    public void onForeground(boolean isForeground) {

    }

    @Override
    public boolean isAlive() {
        return isAlive;
    }

    public boolean isForeground() {
        return AppActiveMatrixDelegate.INSTANCE.isAppForeground();
    }
}
