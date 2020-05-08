package com.tencent.mm.performance.jni.egl;

import android.opengl.EGL14;
import android.util.Log;

import com.tencent.mm.performance.jni.AbsHook;
import com.tencent.mm.performance.jni.LibWxPerfManager;

import java.util.ArrayList;
import java.util.List;

public class EglHook extends AbsHook {

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static final EglHook INSTANCE = new EglHook();
    public static List<OnChangeListener> listeners = new ArrayList<>();

    private EglHook() {
    }

    private native void startHook();

    @Override
    protected void onConfigure() {

    }

    @Override
    protected void onHook() {
        startHook();
    }

    public void addListener(OnChangeListener l) {
        if (l == null) {
            return;
        }

        synchronized (listeners) {
            listeners.add(l);
        }
    }

    public void removeListener(OnChangeListener l) {
        if (l == null) {
            return;
        }

        synchronized (listeners) {
            listeners.remove(l);
        }
    }

    public static void onCreateEglContext(long eglContext, long nativeAddr, String javaStack) {
        EglContextMonitor newEgl = new EglContextMonitor(eglContext, javaStack, nativeAddr);

        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreateEglContext(newEgl);
            }
        }
    }

    public static void onDeleteEglContext(long eglContext) {
        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onDeleteEglContext(eglContext);
            }
        }
    }

    public interface OnChangeListener {
        void onCreateEglContext(EglContextMonitor eglContextMonitor);

        void onDeleteEglContext(long eglContextId);
    }
}
