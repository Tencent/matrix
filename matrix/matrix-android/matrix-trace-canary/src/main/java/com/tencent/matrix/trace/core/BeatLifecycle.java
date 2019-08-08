package com.tencent.matrix.trace.core;

public interface BeatLifecycle {

    void onStart();

    void onStop();

    boolean isAlive();
}
