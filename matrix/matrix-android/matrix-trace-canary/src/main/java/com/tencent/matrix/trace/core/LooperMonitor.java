package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.support.annotation.CallSuper;
import android.util.Printer;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.HashSet;

public class LooperMonitor implements MessageQueue.IdleHandler {

    private static final HashSet<LooperDispatchListener> listeners = new HashSet<>();
    private static final String TAG = "Matrix.LooperMonitor";
    private static Printer printer;
    private static final long CHECK_TIME = 1 * 60 * 1000;
    private long lastCheckPrinterTime = 0;
    public static Printer testPrinter = null;

    public abstract static class LooperDispatchListener {

        boolean isHasDispatchStart = false;

        boolean isValid() {
            return false;
        }

        @CallSuper
        void dispatchStart() {
            this.isHasDispatchStart = true;
        }

        @CallSuper
        void dispatchEnd() {
            this.isHasDispatchStart = false;
        }
    }

    private static final LooperMonitor monitor = new LooperMonitor();

    private LooperMonitor() {
        resetPrinter();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Looper.getMainLooper().getQueue().addIdleHandler(this);
        } else {
            try {
                MessageQueue queue = reflectObject(Looper.getMainLooper(), "mQueue");
                queue.addIdleHandler(this);
            } catch (Exception ex) {
                MatrixLog.printErrStackTrace(TAG, ex, "reflectObject mainLoop fail");
            }
        }
    }

    @Override
    public boolean queueIdle() {
        if (SystemClock.uptimeMillis() - lastCheckPrinterTime >= CHECK_TIME) {
            resetPrinter();
            lastCheckPrinterTime = SystemClock.uptimeMillis();
        }
        return true;
    }

    private static boolean isReflectLoggingError = false;

    private static void resetPrinter() {
        Printer originPrinter = null;
        try {
            if (!isReflectLoggingError) {
                originPrinter = reflectObject(Looper.getMainLooper(), "mLogging");
                if (originPrinter == printer && null != printer) {
                    return;
                }
                if (null != printer) {
                    MatrixLog.w(TAG, "[resetPrinter] maybe looper printer was replace other!");
                }
            }

            final Printer finalOriginPrinter = originPrinter;
            Looper.getMainLooper().setMessageLogging(printer = new Printer() {
                boolean isHasChecked = false;
                boolean isValid = false;

                @Override
                public void println(String x) {
                    if (null != finalOriginPrinter) {
                        finalOriginPrinter.println(x);
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
                        if (null != testPrinter) {
                            testPrinter.println(x);
                        }
                    }

                }
            });
        } catch (Exception e) {
            isReflectLoggingError = true;
            MatrixLog.e(TAG, e.toString());
        }

    }

    public static void register(LooperDispatchListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    public static void unregister(LooperDispatchListener listener) {
        if (null == listener) {
            return;
        }
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }


    private static void dispatch(boolean isBegin) {

        for (LooperDispatchListener listener : listeners) {
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

    private static <T> T reflectObject(Object instance, String name) throws Exception {
        Field field = instance.getClass().getDeclaredField(name);
        field.setAccessible(true);
        return (T) field.get(instance);
    }

}
