package com.tencent.matrix.trace.tracer;

import com.tencent.matrix.trace.listeners.LooperObserver;

public abstract class Tracer extends LooperObserver implements ITracer {

    private volatile boolean isAlive = false;

    protected abstract void onAlive();

    protected abstract void onDead();

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
