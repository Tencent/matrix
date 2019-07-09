package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.support.annotation.CallSuper;
import android.util.Printer;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.HashSet;
import java.util.Objects;

public class LooperMonitor implements MessageQueue.IdleHandler {

    private final HashSet<LooperDispatchListener> listeners = new HashSet<>();
    private static final String TAG = "Matrix.LooperMonitor";
    private static LooperPrinter printer;
    private Looper looper;

    public abstract static class LooperDispatchListener {

        boolean isHasDispatchStart = false;

        boolean isValid() {
            return false;
        }


        public void dispatchStart() {

        }

        @CallSuper
        public void onDispatchStart(String x) {
            this.isHasDispatchStart = true;
            dispatchStart();
        }

        @CallSuper
        public void onDispatchEnd(String x) {
            this.isHasDispatchStart = false;
            dispatchEnd();
        }


        public void dispatchEnd() {
        }
    }

    private static final LooperMonitor mainMonitor = new LooperMonitor();

    public static void register(LooperDispatchListener listener) {
        synchronized (mainMonitor.listeners) {
            mainMonitor.listeners.add(listener);
        }
    }

    public void addListener(LooperDispatchListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    public void removeListener(LooperDispatchListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    public void onRelease() {
        synchronized (listeners) {
            listeners.clear();
        }
        looper.setMessageLogging(printer.origin);
    }

    public static void unregister(LooperDispatchListener listener) {
        synchronized (mainMonitor.listeners) {
            mainMonitor.listeners.remove(listener);
        }
    }

    public LooperMonitor(Looper looper) {
        this.looper = looper;
        Objects.requireNonNull(looper);
        resetPrinter();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            looper.getQueue().addIdleHandler(this);
        } else {
            MessageQueue queue = reflectObject(looper, "mQueue");
            queue.addIdleHandler(this);
        }

    }

    private LooperMonitor() {
        this(Looper.getMainLooper());
    }

    @Override
    public boolean queueIdle() {
        resetPrinter();
        return true;
    }


    private void resetPrinter() {
        final Printer originPrinter = reflectObject(looper, "mLogging");
        if (originPrinter == printer && null != printer) {
            return;
        }
        if (null != printer) {
            MatrixLog.w(TAG, "[resetPrinter] maybe looper printer was replace other!");
        }
        looper.setMessageLogging(printer = new LooperPrinter(originPrinter));
    }

    class LooperPrinter implements Printer {
        public Printer origin;
        boolean isHasChecked = false;
        boolean isValid = false;

        public LooperPrinter(Printer printer) {
            this.origin = printer;
        }

        @Override
        public void println(String x) {
            if (null != origin) {
                origin.println(x);
            }

            if (!isHasChecked) {
                isValid = x.charAt(0) == '>' || x.charAt(0) == '<';
                isHasChecked = true;
                if (!isValid) {
                    MatrixLog.e(TAG, "[println] Printer is inValid! x:%s", x);
                }
            }

            if (isValid) {
                dispatch(x.charAt(0) == '>');
            }

        }
    }


    private void dispatch(boolean isBegin) {

        for (LooperDispatchListener listener : mainMonitor.listeners) {
            if (listener.isValid()) {
                if (isBegin) {
                    if (!listener.isHasDispatchStart) {
                        listener.dispatchStart();
                    }
                } else {
                    if (listener.isHasDispatchStart) {
                        listener.dispatchEnd();
                    }
                }
            } else if (!isBegin && listener.isHasDispatchStart) {
                listener.dispatchEnd();
            }
        }

    }

    private static <T> T reflectObject(Object instance, String name) {
        try {
            Field field = instance.getClass().getDeclaredField(name);
            field.setAccessible(true);
            return (T) field.get(instance);
        } catch (Exception e) {
            e.printStackTrace();
            MatrixLog.e(TAG, e.toString());
        }
        return null;
    }

}
