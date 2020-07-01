package com.tencent.mm.performance.jni.egl;


import com.tencent.mm.performance.jni.AbsHook;
import com.tencent.mm.performance.jni.LibWxPerfManager;
import com.tencent.stubs.logger.Log;

import java.util.ArrayList;
import java.util.List;

public class EglHook extends AbsHook {

    private static final String TAG = "Cc1over-debug";

    private static ILog iLog = new ILog.Default();

    static {
        LibWxPerfManager.INSTANCE.init();
    }

    public static final EglHook INSTANCE = new EglHook();
    private static final List<OnChangeListener> listeners = new ArrayList<>();

    private EglHook() {
    }

    private native void startHook();

    @Override
    protected void onConfigure() {

    }

    public static void initILog(ILog log) {
        iLog = log;
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

        iLog.e(TAG, "onCreateEglContext callback");
        iLog.e(TAG, javaStack);

        EglResourceMonitor newEgl = new EglResourceMonitor(eglContext, javaStack, nativeAddr);

        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreateEglContext(newEgl);
            }
        }
    }

    public static void onDeleteEglSurface(long eglSurface) {

        iLog.e(TAG, "onDeleteEglSurface callback");

        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onDeleteEglSurface(eglSurface);
            }
        }
    }

    public static void onDeleteEglContext(long eglContext) {

        iLog.e(TAG, "onDeleteEglContext callback");

        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onDeleteEglContext(eglContext);
            }
        }
    }

    public static void onCreateEglWindowSurface(long eglSurface, long nativeAddr, String javaStack) {

        iLog.e(TAG, "onCreateEglWindowSurface callback");
        iLog.e(TAG, javaStack);

        EglResourceMonitor newEgl = new EglResourceMonitor(eglSurface, javaStack, nativeAddr);

        synchronized (listeners) {
            if(listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreateEglWindowSurface(newEgl);
            }
        }
    }

    public static void onCreatePbufferSurface(long eglSurface, long nativeAddr, String javaStack) {

        iLog.e(TAG, "onCreatePbufferSurface callback");
        iLog.e(TAG, javaStack);

        EglResourceMonitor newEgl = new EglResourceMonitor(eglSurface, javaStack, nativeAddr);

        synchronized (listeners) {
            if(listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreatePbufferSurface(newEgl);
            }
        }
    }

    public interface OnChangeListener {

        void onCreateEglContext(EglResourceMonitor eglContextMonitor);

        void onDeleteEglContext(long eglContextId);

        void onCreateEglWindowSurface(EglResourceMonitor eglContextMonitor);

        void onCreatePbufferSurface(EglResourceMonitor eglContextMonitor);

        void onDeleteEglSurface(long eglSurfaceId);

    }

    public interface ILog {

        void v(String tag, String info);

        void i(String tag, String info);

        void e(String tag, String info);

        void w(String tag, String info);

        void d(String tag, String info);


        class Default implements  ILog{

            @Override
            public void v(String tag, String info) {

            }

            @Override
            public void i(String tag, String info) {

            }

            @Override
            public void e(String tag, String info) {

            }

            @Override
            public void w(String tag, String info) {

            }

            @Override
            public void d(String tag, String info) {

            }
        }

    }

}
