package com.tencent.matrix.trace.tracer;

public abstract class Tracer implements ITracer {

    private volatile boolean isAlive = false;

    protected abstract void onAlive();

    protected abstract void onDead();

    @Override
    final public void onStartTrace() {
        if (!isAlive) {
            this.isAlive = true;
            onAlive();
        }
    }

    @Override
    final public void onCloseTrace() {
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
