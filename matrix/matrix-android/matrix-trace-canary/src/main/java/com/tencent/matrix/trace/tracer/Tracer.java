package com.tencent.matrix.trace.tracer;

import android.support.annotation.CallSuper;

import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.util.MatrixLog;

public abstract class Tracer extends LooperObserver implements ITracer {

    private volatile boolean isAlive = false;
    private static final String TAG = "Matrix.Tracer";

    @CallSuper
    protected void onAlive() {
        MatrixLog.i(TAG, "[onAlive] %s", this.getClass().getSimpleName());

    }

    @CallSuper
    protected void onDead() {
        MatrixLog.i(TAG, "[onDead] %s", this.getClass().getSimpleName());
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

    private volatile boolean isForeground;


    @Override
    public void onForeground(boolean isForeground) {
        this.isForeground = isForeground;
    }

    public boolean isForeground() {
        return isForeground;
    }
}
