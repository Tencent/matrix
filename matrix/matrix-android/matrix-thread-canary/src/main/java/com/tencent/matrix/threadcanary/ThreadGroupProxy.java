package com.tencent.matrix.threadcanary;

public class ThreadGroupProxy extends ThreadGroup {

    public ThreadGroupProxy(String name) {
        super(name);
    }

    public ThreadGroupProxy(ThreadGroup parent, String name) {
        super(parent, name);
    }

    @Override
    public synchronized boolean isDestroyed() {
        return super.isDestroyed();
    }

}
