package com.tencent.wxperf.jni.egl;


import android.support.annotation.Keep;

import com.tencent.wxperf.jni.AbsHook;

import java.util.ArrayList;
import java.util.List;

public class EglHook extends AbsHook {

    private static final String TAG = "Cc1over-debug";

    private static ILog iLog = new ILog.Default();

    public static final EglHook INSTANCE = new EglHook();
    private static final List<OnChangeListener> listeners = new ArrayList<>();

    private EglHook() {
    }

    private native void startHook();

    @Override
    protected void onConfigure() {

    }

    /**
     * Deprecated: use {@link com.tencent.stubs.logger.Log} instead
     * @param log
     */
    @Deprecated
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

    @Keep
    public static void onCreateEglContext(long eglContext) {

        iLog.e(TAG, "onCreateEglContext callback");

        EglResourceMonitor newEgl = new EglResourceMonitor(eglContext);

        synchronized (listeners) {
            if (listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreateEglContext(newEgl);
            }
        }
    }

    @Keep
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

    @Keep
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

    @Keep
    public static void onCreateEglWindowSurface(long eglSurface) {

        iLog.e(TAG, "onCreateEglWindowSurface callback");

        EglResourceMonitor newEgl = new EglResourceMonitor(eglSurface);

        synchronized (listeners) {
            if(listeners.size() == 0) {
                return;
            }

            for (OnChangeListener l : listeners) {
                l.onCreateEglWindowSurface(newEgl);
            }
        }
    }

    @Keep
    public static void onCreatePbufferSurface(long eglSurface) {

        iLog.e(TAG, "onCreatePbufferSurface callback");

        EglResourceMonitor newEgl = new EglResourceMonitor(eglSurface);

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

        @Keep
        void onCreateEglContext(EglResourceMonitor eglContextMonitor);

        @Keep
        void onDeleteEglContext(long eglContextId);

        @Keep
        void onCreateEglWindowSurface(EglResourceMonitor eglContextMonitor);

        @Keep
        void onCreatePbufferSurface(EglResourceMonitor eglContextMonitor);

        @Keep
        void onDeleteEglSurface(long eglSurfaceId);

    }

    /**
     * Deprecated: use {@link com.tencent.stubs.logger.Log} instead
     */
    @Deprecated
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
