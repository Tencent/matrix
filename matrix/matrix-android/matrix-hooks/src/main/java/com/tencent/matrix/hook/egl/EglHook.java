package com.tencent.matrix.hook.egl;




import androidx.annotation.Keep;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.hook.AbsHook;

import java.util.ArrayList;
import java.util.List;

public class EglHook extends AbsHook {

    private static final String TAG = "Matrix.EglHook";

    public static final EglHook INSTANCE = new EglHook();
    private static final List<OnChangeListener> listeners = new ArrayList<>();

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

    @Keep
    public static void onCreateEglContext(long eglContext) {

        MatrixLog.i(TAG, "onCreateEglContext callback");

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

        MatrixLog.i(TAG, "onDeleteEglSurface callback");

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

        MatrixLog.i(TAG, "onDeleteEglContext callback");

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

        MatrixLog.i(TAG, "onCreateEglWindowSurface callback");

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

        MatrixLog.i(TAG, "onCreatePbufferSurface callback");

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

}
