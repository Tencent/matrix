package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.util.Printer;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.HashSet;

public class LooperMonitor implements MessageQueue.IdleHandler {

    private static final HashSet<LooperDispatchListener> listeners = new HashSet<>();
    private static final String TAG = "Matrix.LooperMonitor";
    private static Printer printer;


    public interface LooperDispatchListener {

        boolean isValid();

        void dispatchStart();

        void dispatchEnd();
    }

    private static final LooperMonitor monitor = new LooperMonitor();

    private LooperMonitor() {
        resetPrinter();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Looper.getMainLooper().getQueue().addIdleHandler(this);
        } else {
            MessageQueue queue = reflectObject(Looper.getMainLooper(), "mQueue");
            queue.addIdleHandler(this);
        }
    }

    @Override
    public boolean queueIdle() {
        resetPrinter();
        return true;
    }


    private static void resetPrinter() {
        final Printer originPrinter = reflectObject(Looper.getMainLooper(), "mLogging");
        if (originPrinter == printer && null != printer) {
            MatrixLog.v(TAG, "[resetPrinter] self printer is right!");
            return;
        }
        if (null != printer) {
            MatrixLog.w(TAG, "[resetPrinter] maybe looper printer was replace other!");
        }
        Looper.getMainLooper().setMessageLogging(printer = new Printer() {
            boolean hasDispatchBegin = false;
            boolean isHandleMessageEnd = true;

            @Override
            public void println(String x) {
                if (null != originPrinter) {
                    originPrinter.println(x);
                }
                isHandleMessageEnd = !isHandleMessageEnd;

                if (!isHandleMessageEnd) {
                    hasDispatchBegin = true;
                    dispatch(true);
                } else {
                    hasDispatchBegin = false;
                    dispatch(false);
                }
            }
        });
    }

    public static void register(LooperDispatchListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }


    private static void dispatch(boolean isBegin) {
        for (LooperDispatchListener listener : listeners) {
            if (listener.isValid()) {
                if (isBegin) {
                    listener.dispatchStart();
                } else {
                    listener.dispatchEnd();
                }
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
