/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.util.Log;
import android.util.Printer;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;

import com.tencent.matrix.trace.listeners.ILooperListener;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.ReflectUtils;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Objects;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

public class LooperMonitor implements MessageQueue.IdleHandler {
    private static final String TAG = "Matrix.LooperMonitor";
    private static final Map<Looper, LooperMonitor> sLooperMonitorMap = new ConcurrentHashMap<>();
    private static final LooperMonitor sMainMonitor = LooperMonitor.of(Looper.getMainLooper());

    private static final HandlerThread historyMsgHandlerThread = MatrixHandlerThread.getNewHandlerThread("historyMsgHandlerThread", HandlerThread.NORM_PRIORITY);
    private static final Handler historyMsgHandler = new Handler(historyMsgHandlerThread.getLooper());
    private static final int HISTORY_QUEUE_MAX_SIZE = 200;
    private static final int RECENT_QUEUE_MAX_SIZE = 5000;

    private final Queue<M> anrHistoryMQ = new ConcurrentLinkedQueue<>();
    private final Queue<M> recentMsgQ = new ConcurrentLinkedQueue<>();

    private long messageStartTime = 0;
    private String latestMsgLog = "";
    private long recentMCount = 0;
    private long recentMDuration = 0;
    private boolean denseMsgTracer = false;
    private boolean historyMsgRecorder = false;

    @Deprecated
    public abstract static class LooperDispatchListener {

        boolean isHasDispatchStart = false;
        boolean historyMsgRecorder = false;
        boolean denseMsgTracer = false;

        public LooperDispatchListener(boolean historyMsgRecorder, boolean denseMsgTracer) {
            this.historyMsgRecorder = historyMsgRecorder;
            this.denseMsgTracer = denseMsgTracer;
        }

        public LooperDispatchListener() {

        }

        public boolean isValid() {
            return false;
        }


        public void dispatchStart() {

        }

        @CallSuper
        public void onDispatchStart(String x) {
            if (!isHasDispatchStart) {
                isHasDispatchStart = true;
                dispatchStart();
            }
        }

        @CallSuper
        public void onDispatchEnd(String x) {
            if (isHasDispatchStart) {
                isHasDispatchStart = false;
                dispatchEnd();
            }
        }


        public void dispatchEnd() {
        }
    }

    private final static class DispatchListenerWrapper {
        private boolean isHasDispatchStart = false;
        private long beginNs;
        private final ILooperListener dispatchListener;

        DispatchListenerWrapper(ILooperListener dispatchListener) {
            this.dispatchListener = dispatchListener;
        }

        public boolean isValid() {
            return dispatchListener.isValid();
        }

        public void onDispatchBegin(String x) {
            if (!isHasDispatchStart) {
                isHasDispatchStart = true;
                beginNs = System.nanoTime();
                dispatchListener.onDispatchBegin(x);
            }
        }

        public void onDispatchEnd(String x) {
            if (isHasDispatchStart) {
                isHasDispatchStart = false;
                dispatchListener.onDispatchEnd(x, beginNs, System.nanoTime());
            }
        }
    }

    public static LooperMonitor getMainMonitor() {
        return sMainMonitor;
    }

    public static LooperMonitor of(@NonNull Looper looper) {
        LooperMonitor looperMonitor = sLooperMonitorMap.get(looper);
        if (looperMonitor == null) {
            looperMonitor = new LooperMonitor(looper);
            sLooperMonitorMap.put(looper, looperMonitor);
        }
        return looperMonitor;
    }

    @Deprecated
    static void register(LooperDispatchListener listener) {
        sMainMonitor.addListener(listener);
    }

    @Deprecated
    static void unregister(LooperDispatchListener listener) {
        sMainMonitor.removeListener(listener);
    }

    public static void register(ILooperListener listener) {
        sMainMonitor.addListener(listener);
    }

    public static void unregister(ILooperListener listener) {
        sMainMonitor.removeListener(listener);
    }

    @Deprecated
    private final HashSet<LooperDispatchListener> oldListeners = new HashSet<>();
    private final Map<ILooperListener, DispatchListenerWrapper> listeners = new HashMap<>();
    private LooperPrinter printer;
    private Looper looper;
    private static final long CHECK_TIME = 60 * 1000L;
    private long lastCheckPrinterTime = 0;

    /**
     * It will be thread-unsafe if you get the listeners and literate.
     */
    @Deprecated
    public HashSet<LooperDispatchListener> getOldListeners() {
        return oldListeners;
    }

    @Deprecated
    public void addListener(LooperDispatchListener listener) {
        synchronized (oldListeners) {
            oldListeners.add(listener);
        }
    }

    @Deprecated
    public void removeListener(LooperDispatchListener listener) {
        synchronized (oldListeners) {
            oldListeners.remove(listener);
        }
    }

    public void addListener(ILooperListener listener) {
        synchronized (listeners) {
            DispatchListenerWrapper wrapper = new DispatchListenerWrapper(listener);
            listeners.put(listener, wrapper);
        }
    }

    public void removeListener(ILooperListener listener) {
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
            synchronized (oldListeners) {
                oldListeners.clear();
            }
            synchronized (listeners) {
                listeners.clear();
            }
            MatrixLog.v(TAG, "[onRelease] %s, origin printer:%s", looper.getThread().getName(), printer.origin);
            removeIdleHandler(looper);
            looper.setMessageLogging(printer.origin);
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

    private void recordMsg(final String log, final long duration) {
        historyMsgHandler.post(new Runnable() {
            @Override
            public void run() {
                enqueueHistoryMQ(new M(log, duration));
            }
        });

        if (denseMsgTracer) {
            historyMsgHandler.post(new Runnable() {
                @Override
                public void run() {
                    enqueueRecentMQ(new M(log, duration));
                }
            });
        }
    }

    private void enqueueRecentMQ(M m) {
        if (recentMsgQ.size() == RECENT_QUEUE_MAX_SIZE) {
            recentMsgQ.poll();
        }
        recentMsgQ.offer(m);

        recentMDuration += m.d;
    }

    private void enqueueHistoryMQ(M m) {
        if (anrHistoryMQ.size() == HISTORY_QUEUE_MAX_SIZE) {
            anrHistoryMQ.poll();
        }
        anrHistoryMQ.offer(m);
    }

    public Queue<M> getHistoryMQ() {
        enqueueHistoryMQ(new M(latestMsgLog, System.currentTimeMillis() - messageStartTime));
        return anrHistoryMQ;
    }

    public Queue<M> getRecentMsgQ() {
        return recentMsgQ;
    }

    public void cleanRecentMQ() {
        recentMsgQ.clear();
        recentMCount = 0;
        recentMDuration = 0;
    }

    public long getRecentMCount() {
        return recentMCount;
    }

    public long getRecentMDuration() {
        return recentMDuration;
    }

    private void dispatch(boolean isBegin, String log) {
        if (isBegin) {
            if (historyMsgRecorder) {
                messageStartTime = System.currentTimeMillis();
                latestMsgLog = log;
                recentMCount++;
            }
            synchronized (oldListeners) {
                for (LooperDispatchListener listener : oldListeners) {
                    if (listener.isValid()) {
                        listener.onDispatchStart(log);
                    }
                }
            }
            synchronized (listeners) {
                for (DispatchListenerWrapper listener : listeners.values()) {
                    if (listener.isValid()) {
                        listener.onDispatchBegin(log);
                    }
                }
            }
        } else {
            if (historyMsgRecorder) {
                recordMsg(log, System.currentTimeMillis() - messageStartTime);
            }
            synchronized (oldListeners) {
                for (LooperDispatchListener listener : oldListeners) {
                    if (listener.isValid()) {
                        listener.onDispatchEnd(log);
                    }
                }
            }
            synchronized (listeners) {
                for (DispatchListenerWrapper listener : listeners.values()) {
                    if (listener.isValid()) {
                        listener.onDispatchEnd(log);
                    }
                }
            }
        }
    }

    public static class M {
        public String l;
        public long d;

        M(String l, long d) {
            this.l = l;
            this.d = d;
        }

        @Override
        public String toString() {
            return "{" + l + " -> " + d + '}';
        }
    }
}
