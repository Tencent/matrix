package com.tencent.matrix.trace.core;

import android.os.Looper;
import android.os.SystemClock;
import android.util.Printer;
import android.view.Choreographer;

import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashSet;

public class UIThreadMonitor implements BeatLifecycle, Runnable {

    public interface IFrameObserver {
        void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCostNs, long animationCostNs, long traversalCostNs);
    }

    public interface ILooperObserver extends IFrameObserver {
        void dispatchBegin(long beginMs, long cpuBeginMs, long token);

        void dispatchEnd(long beginMs, long cpuBeginMs, long endMs, long cpuEndMs, long token, boolean isBelongFrame);
    }

    private static final String TAG = "Matrix.UIThreadMonitor";
    private static final String ADD_CALLBACK = "addCallbackLocked";
    private static final boolean isDebug = true;
    private volatile boolean isAlive = false;
    private boolean isHandleMessageEnd = true;
    private long[] dispatchTimeMs = new long[4];
    private HashSet<IFrameObserver> observers = new HashSet<>();
    private volatile long token = 0L;
    private boolean isBelongFrame = false;

    /**
     * Callback type: Input callback.  Runs first.
     *
     * @hide
     */
    public static final int CALLBACK_INPUT = 0;

    /**
     * Callback type: Animation callback.  Runs before traversals.
     *
     * @hide
     */
    public static final int CALLBACK_ANIMATION = 1;

    /**
     * Callback type: Commit callback.  Handles post-draw operations for the frame.
     * Runs after traversal completes.
     *
     * @hide
     */
    public static final int CALLBACK_TRAVERSAL = 2;

    private static final int CALLBACK_LAST = CALLBACK_TRAVERSAL;

    private final static UIThreadMonitor sInstance = new UIThreadMonitor();
    private TraceConfig config;
    private final Object callbackQueueLock;
    private final Object[] callbackQueues;
    private final Method addTraversalQueue;
    private final Method addInputQueue;
    private final Method addAnimationQueue;
    private final Choreographer choreographer;
    private final long frameIntervalNanos;
    private int[] queueStatus = new int[CALLBACK_LAST + 1];
    private long[] queueCost = new long[CALLBACK_LAST + 1];
    private static final int DO_QUEUE_DEFAULT = 0;
    private static final int DO_QUEUE_BEGIN = 1;
    private static final int DO_QUEUE_END = 2;

    public static UIThreadMonitor getMonitor() {
        return sInstance;
    }

    public void setConfig(TraceConfig config) {
        this.config = config;
    }

    private UIThreadMonitor() {
        assert Thread.currentThread() == Looper.getMainLooper().getThread();
        choreographer = Choreographer.getInstance();
        callbackQueueLock = reflectObject(choreographer, "mLock");
        callbackQueues = reflectObject(choreographer, "mCallbackQueues");

        addInputQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_INPUT], ADD_CALLBACK, long.class, Object.class, Object.class);
        addAnimationQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_ANIMATION], ADD_CALLBACK, long.class, Object.class, Object.class);
        addTraversalQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_TRAVERSAL], ADD_CALLBACK, long.class, Object.class, Object.class);
        frameIntervalNanos = reflectObject(choreographer, "mFrameIntervalNanos");
        final Printer originPrinter = reflectObject(Looper.getMainLooper(), "mLogging");
        Looper.getMainLooper().setMessageLogging(new Printer() {
            boolean hasDispatchBegin = false;

            @Override
            public void println(String x) {
                if (null != originPrinter) {
                    originPrinter.println(x);
                }
                UIThreadMonitor.this.isHandleMessageEnd = !isHandleMessageEnd;

                if (!isAlive && !hasDispatchBegin) {
                    return;
                }

                if (!isHandleMessageEnd) {
                    hasDispatchBegin = true;
                    dispatchBegin();
                } else {
                    hasDispatchBegin = false;
                    dispatchEnd();
                }
            }
        });

        if (config.isDevEnv()) {
            addObserver(new IFrameObserver() {
                @Override
                public void doFrame(String focusedActivityName, long start, long end, long frameCostMs, long inputCost, long animationCost, long traversalCost) {
                    MatrixLog.i(TAG, "activityName[%s] frame cost:%sns [%s|%s|%s]", focusedActivityName, frameCostMs, inputCost, animationCost, traversalCost);
                }
            });
        }

        MatrixLog.i(TAG, "[UIThreadMonitor] %s %s %s %s %s", callbackQueueLock == null, callbackQueues == null, addInputQueue == null, addTraversalQueue == null, addAnimationQueue == null);
    }

    private void addFrameCallback(int type, Runnable callback, boolean isAddHeader) {
        if (!isAlive && type == CALLBACK_INPUT) {
            MatrixLog.w(TAG, "[addFrameCallback] UIThreadMonitor is not alive!");
            return;
        }
        try {
            synchronized (callbackQueueLock) {
                Method method = null;
                switch (type) {
                    case CALLBACK_INPUT:
                        method = addInputQueue;
                        break;
                    case CALLBACK_ANIMATION:
                        method = addAnimationQueue;
                        break;
                    case CALLBACK_TRAVERSAL:
                        method = addTraversalQueue;
                        break;
                }
                if (null != method) {
                    method.invoke(callbackQueues[type], !isAddHeader ? SystemClock.uptimeMillis() : -1, callback, null);
                }
            }
        } catch (Exception e) {
            MatrixLog.e(TAG, e.toString());
        }
    }

    public long getFrameIntervalNanos() {
        return frameIntervalNanos;
    }

    public void addObserver(IFrameObserver observer) {
        if (!isAlive) {
            onStart();
        }
        synchronized (observers) {
            observers.add(observer);
        }
    }

    public void removeObserver(IFrameObserver observer) {
        synchronized (observers) {
            observers.remove(observer);
            if (observers.isEmpty()) {
                onStop();
            }
        }
    }

    public long getQueueCost(int type, long token) {
        if (token != this.token) {
            return -1;
        }
        return queueStatus[type] == DO_QUEUE_END ? queueCost[type] : 0;
    }

    private <T> T reflectObject(Object instance, String name) {
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

    private Method reflectChoreographerMethod(Object instance, String name, Class<?>... argTypes) {
        try {
            Method method = instance.getClass().getDeclaredMethod(name, argTypes);
            method.setAccessible(true);
            return method;
        } catch (Exception e) {
            MatrixLog.e(TAG, e.toString());
        }
        return null;
    }

    private void dispatchBegin() {
        token = dispatchTimeMs[0] = SystemClock.uptimeMillis();
        dispatchTimeMs[2] = SystemClock.currentThreadTimeMillis();
        AppMethodBeat.i(AppMethodBeat.METHOD_ID_DISPATCH);

        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                if (observer instanceof ILooperObserver) {
                    ILooperObserver looperObserver = (ILooperObserver) observer;
                    looperObserver.dispatchBegin(dispatchTimeMs[0], dispatchTimeMs[2], token);
                }
            }
        }
    }

    private void doFrameBegin(long token) {
        this.isBelongFrame = true;
    }

    private void doFrameEnd(long token) {
        for (int i : queueStatus) {
            if (i != DO_QUEUE_END) {
                throw new RuntimeException(String.format("UIThreadMonitor happens type[%s] != DO_QUEUE_END", i));
            }
        }
        queueStatus = new int[CALLBACK_LAST + 1];

        long start = token;
        long end = SystemClock.uptimeMillis();
        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                observer.doFrame(AppMethodBeat.getFocusedActivity(), start, end, end - start, queueCost[CALLBACK_INPUT], queueCost[CALLBACK_ANIMATION], queueCost[CALLBACK_TRAVERSAL]);
            }
        }
    }

    private void dispatchEnd() {
        if (isBelongFrame) {
            doQueueEnd(CALLBACK_TRAVERSAL);
            doFrameEnd(token);
            addFrameCallback(CALLBACK_INPUT, this, true);
        }
        AppMethodBeat.o(AppMethodBeat.METHOD_ID_DISPATCH);

        this.isBelongFrame = false;
        dispatchTimeMs[3] = SystemClock.currentThreadTimeMillis();
        dispatchTimeMs[1] = SystemClock.uptimeMillis();

        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                if (observer instanceof ILooperObserver) {
                    ILooperObserver looperObserver = (ILooperObserver) observer;
                    looperObserver.dispatchEnd(dispatchTimeMs[0], dispatchTimeMs[2], dispatchTimeMs[1], dispatchTimeMs[3], token, isBelongFrame);
                }
            }
        }

    }

    private void doQueueBegin(int type) {
        queueStatus[type] = DO_QUEUE_BEGIN;
        queueCost[type] = System.nanoTime();
    }

    private void doQueueEnd(int type) {
        queueStatus[type] = DO_QUEUE_END;
        queueCost[type] = System.nanoTime() - queueCost[type];
    }

    @Override
    public void onStart() {
        if (!isAlive) {
            MatrixLog.i(TAG, "[onStart] %s", Utils.getStack());
            this.isAlive = true;
            addFrameCallback(CALLBACK_INPUT, this, true);
        }
    }

    @Override
    public void run() {
        final long start = System.nanoTime();
        try {
            doFrameBegin(token);
            doQueueBegin(CALLBACK_INPUT);

            addFrameCallback(CALLBACK_ANIMATION, new Runnable() {

                @Override
                public void run() {
                    doQueueEnd(CALLBACK_INPUT);
                    doQueueBegin(CALLBACK_ANIMATION);
                }
            }, true);

            addFrameCallback(CALLBACK_TRAVERSAL, new Runnable() {

                @Override
                public void run() {
                    doQueueEnd(CALLBACK_ANIMATION);
                    doQueueBegin(CALLBACK_TRAVERSAL);
                }
            }, true);

        } finally {
            if (config.isDevEnv()) {
                MatrixLog.d(TAG, "[run] inner cost:%sns", System.nanoTime() - start);
            }
        }
    }


    @Override
    public void onStop() {
        if (isAlive) {
            this.isAlive = false;
            MatrixLog.i(TAG, "[onStop] %s", Utils.getStack());
        }
    }
}
