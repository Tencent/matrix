package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;

import android.util.Log;
import android.util.Printer;

import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.ReflectUtils;

import java.util.HashSet;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class LooperMonitor implements MessageQueue.IdleHandler {
    private static final String TAG = "Matrix.LooperMonitor";
    private static final Map<Looper, LooperMonitor> sLooperMonitorMap = new ConcurrentHashMap<>();
    private static final LooperMonitor sMainMonitor = LooperMonitor.of(Looper.getMainLooper());

    public abstract static class LooperDispatchListener {

        boolean isHasDispatchStart = false;

        public boolean isValid() {
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

    public static LooperMonitor of(@NonNull Looper looper) {
        LooperMonitor looperMonitor = sLooperMonitorMap.get(looper);
        if (looperMonitor == null) {
            looperMonitor = new LooperMonitor(looper);
            sLooperMonitorMap.put(looper, looperMonitor);
        }
        return looperMonitor;
    }

    static void register(LooperDispatchListener listener) {
        sMainMonitor.addListener(listener);
    }

    static void unregister(LooperDispatchListener listener) {
        sMainMonitor.removeListener(listener);
    }

    private final HashSet<LooperDispatchListener> listeners = new HashSet<>();
    private LooperPrinter printer;
    private Looper looper;
    private static final long CHECK_TIME = 60 * 1000L;
    private long lastCheckPrinterTime = 0;

    /**
     * It will be thread-unsafe if you get the listeners and literate.
     */
    @Deprecated
    public HashSet<LooperDispatchListener> getListeners() {
        return listeners;
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

    private LooperMonitor(Looper looper) {
        Objects.requireNonNull(looper);
        this.looper = looper;
        resetPrinter();
        addIdleHandler(looper);
    }

    public Looper getLooper() {
        return looper;
    }

    @Override
    public boolean queueIdle() {
        if (SystemClock.uptimeMillis() - lastCheckPrinterTime >= CHECK_TIME) {
            resetPrinter();
            lastCheckPrinterTime = SystemClock.uptimeMillis();
        }
        return true;
    }

    public synchronized void onRelease() {
        if (printer != null) {
            synchronized (listeners) {
                listeners.clear();
            }
            MatrixLog.v(TAG, "[onRelease] %s, origin printer:%s", looper.getThread().getName(), printer.origin);
            looper.setMessageLogging(printer.origin);
            removeIdleHandler(looper);
            looper = null;
            printer = null;
        }
    }

    private static boolean isReflectLoggingError = false;

    private synchronized void resetPrinter() {
        Printer originPrinter = null;
        try {
            if (!isReflectLoggingError) {
                originPrinter = ReflectUtils.get(looper.getClass(), "mLogging", looper);
                if (originPrinter == printer && null != printer) {
                    return;
                }
                // Fix issues that printer loaded by different classloader
                if (originPrinter != null && printer != null) {
                    if (originPrinter.getClass().getName().equals(printer.getClass().getName())) {
                        MatrixLog.w(TAG, "LooperPrinter might be loaded by different classloader"
                                + ", my = " + printer.getClass().getClassLoader()
                                + ", other = " + originPrinter.getClass().getClassLoader());
                        return;
                    }
                }
            }
        } catch (Exception e) {
            isReflectLoggingError = true;
            Log.e(TAG, "[resetPrinter] %s", e);
        }

        if (null != printer) {
            MatrixLog.w(TAG, "maybe thread:%s printer[%s] was replace other[%s]!",
                    looper.getThread().getName(), printer, originPrinter);
        }
        looper.setMessageLogging(printer = new LooperPrinter(originPrinter));
        if (null != originPrinter) {
            MatrixLog.i(TAG, "reset printer, originPrinter[%s] in %s", originPrinter, looper.getThread().getName());
        }
    }

    private synchronized void removeIdleHandler(Looper looper) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            looper.getQueue().removeIdleHandler(this);
        } else {
            try {
                MessageQueue queue = ReflectUtils.get(looper.getClass(), "mQueue", looper);
                queue.removeIdleHandler(this);
            } catch (Exception e) {
                Log.e(TAG, "[removeIdleHandler] %s", e);
            }

        }
    }

    private synchronized void addIdleHandler(Looper looper) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            looper.getQueue().addIdleHandler(this);
        } else {
            try {
                MessageQueue queue = ReflectUtils.get(looper.getClass(), "mQueue", looper);
                queue.addIdleHandler(this);
            } catch (Exception e) {
                Log.e(TAG, "[removeIdleHandler] %s", e);
            }
        }
    }


    class LooperPrinter implements Printer {
        public Printer origin;
        boolean isHasChecked = false;
        boolean isValid = false;

        LooperPrinter(Printer printer) {
            this.origin = printer;
        }

        @Override
        public void println(String x) {
            if (null != origin) {
                origin.println(x);
                if (origin == this) {
                    throw new RuntimeException(TAG + " origin == this");
                }
            }

            if (!isHasChecked) {
                isValid = x.charAt(0) == '>' || x.charAt(0) == '<';
                isHasChecked = true;
                if (!isValid) {
                    MatrixLog.e(TAG, "[println] Printer is inValid! x:%s", x);
                }
            }

            if (isValid) {
                dispatch(x.charAt(0) == '>', x);
            }

        }
    }

    private void dispatch(boolean isBegin, String log) {
        synchronized (listeners) {
            for (LooperDispatchListener listener : listeners) {
                if (listener.isValid()) {
                    if (isBegin) {
                        if (!listener.isHasDispatchStart) {
                            listener.onDispatchStart(log);
                        }
                    } else {
                        if (listener.isHasDispatchStart) {
                            listener.onDispatchEnd(log);
                        }
                    }
                } else if (!isBegin && listener.isHasDispatchStart) {
                    listener.dispatchEnd();
                }
            }
        }
    }
}
