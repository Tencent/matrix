package com.tencent.matrix.batterycanary.utils;

import java.lang.ref.WeakReference;

import androidx.annotation.NonNull;

/**
 * @author Kaede
 * @since 2022/1/4
 */
public abstract class ThreadSafeReference<T> {
    ThreadLocal<WeakReference<T>> mThreadLocalRef;

    @NonNull
    public abstract T onCreate();

    @NonNull
    public T safeGet() {
        if (mThreadLocalRef != null) {
            WeakReference<T> ref = mThreadLocalRef.get();
            if (ref != null) {
                T target = ref.get();
                if (target != null) {
                    return target;
                }
            }
        }
        T target = onCreate();
        WeakReference<T> ref = new WeakReference<>(target);
        mThreadLocalRef = new ThreadLocal<>();
        mThreadLocalRef.set(ref);
        return target;
    }
}
