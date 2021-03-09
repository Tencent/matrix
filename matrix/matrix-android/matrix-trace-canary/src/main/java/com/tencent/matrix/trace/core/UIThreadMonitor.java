package com.tencent.matrix.trace.core;

import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Choreographer;

import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.DeviceUtil;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.ReflectUtils;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;

public class UIThreadMonitor implements BeatLifecycle, Runnable {

    private static final String TAG = "Matrix.UIThreadMonitor";
    private static final String ADD_CALLBACK = "addCallbackLocked";
    private volatile boolean isAlive = false;
    private long[] dispatchTimeMs = new long[4];
    private final HashSet<LooperObserver> observers = new HashSet<>();
    private volatile long token = 0L;
    private boolean isVsyncFrame = false;
    // The time of the oldest input event
    private static final int OLDEST_INPUT_EVENT = 3;

    // The time of the newest input event
    private static final int NEWEST_INPUT_EVENT = 4;

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

    /**
     * never do queue end code
     */
    public static final int DO_QUEUE_END_ERROR = -100;

    private static final int CALLBACK_LAST = CALLBACK_TRAVERSAL;

    private final static UIThreadMonitor sInstance = new UIThreadMonitor();
    private TraceConfig config;
    private Object callbackQueueLock;
    private Object[] callbackQueues;
    private Method addTraversalQueue;
    private Method addInputQueue;
    private Method addAnimationQueue;
    private Choreographer choreographer;
    private Object vsyncReceiver;
    private long frameIntervalNanos = 16666666;
    private int[] queueStatus = new int[CALLBACK_LAST + 1];
    private boolean[] callbackExist = new boolean[CALLBACK_LAST + 1]; // ABA
    private long[] queueCost = new long[CALLBACK_LAST + 1];
    private static final int DO_QUEUE_DEFAULT = 0;
    private static final int DO_QUEUE_BEGIN = 1;
    private static final int DO_QUEUE_END = 2;
    private boolean isInit = false;

    public static UIThreadMonitor getMonitor() {
        return sInstance;
    }

    public boolean isInit() {
        return isInit;
    }

    public void init(TraceConfig config) {
        if (Thread.currentThread() != Looper.getMainLooper().getThread()) {
            throw new AssertionError("must be init in main thread!");
        }
        this.config = config;
        choreographer = Choreographer.getInstance();
        callbackQueueLock = ReflectUtils.reflectObject(choreographer, "mLock", new Object());
        callbackQueues = ReflectUtils.reflectObject(choreographer, "mCallbackQueues", null);
        if (null != callbackQueues) {
            addInputQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_INPUT], ADD_CALLBACK, long.class, Object.class, Object.class);
            addAnimationQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_ANIMATION], ADD_CALLBACK, long.class, Object.class, Object.class);
            addTraversalQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_TRAVERSAL], ADD_CALLBACK, long.class, Object.class, Object.class);
        }
        vsyncReceiver = ReflectUtils.reflectObject(choreographer, "mDisplayEventReceiver", null);
        frameIntervalNanos = ReflectUtils.reflectObject(choreographer, "mFrameIntervalNanos", Constants.DEFAULT_FRAME_DURATION);

        LooperMonitor.register(new LooperMonitor.LooperDispatchListener() {
            @Override
            public boolean isValid() {
                return isAlive;
            }

            @Override
            public void dispatchStart() {
                super.dispatchStart();
                UIThreadMonitor.this.dispatchBegin();
            }

            @Override
            public void dispatchEnd() {
                super.dispatchEnd();
                UIThreadMonitor.this.dispatchEnd();
            }

        });
        this.isInit = true;
        MatrixLog.i(TAG, "[UIThreadMonitor] %s %s %s %s %s %s frameIntervalNanos:%s", callbackQueueLock == null, callbackQueues == null,
                addInputQueue == null, addTraversalQueue == null, addAnimationQueue == null, vsyncReceiver == null, frameIntervalNanos);


        if (config.isDevEnv()) {
            addObserver(new LooperObserver() {
                @Override
                public void doFrame(String focusedActivity, long startNs, long endNs, boolean isVsyncFrame, long intendedFrameTimeNs, long inputCostNs, long animationCostNs, long traversalCostNs) {
                    MatrixLog.i(TAG, "focusedActivity[%s] frame cost:%sms isVsyncFrame=%s intendedFrameTimeNs=%s [%s|%s|%s]ns",
                            focusedActivity, (endNs - startNs) / Constants.TIME_MILLIS_TO_NANO, isVsyncFrame, intendedFrameTimeNs, inputCostNs, animationCostNs, traversalCostNs);
                }
            });
        }
    }

    private synchronized void addFrameCallback(int type, Runnable callback, boolean isAddHeader) {
        if (callbackExist[type]) {
            MatrixLog.w(TAG, "[addFrameCallback] this type %s callback has exist! isAddHeader:%s", type, isAddHeader);
            return;
        }

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
                    callbackExist[type] = true;
                }
            }
        } catch (Exception e) {
            MatrixLog.e(TAG, e.toString());
        }
    }

    public long getFrameIntervalNanos() {
        return frameIntervalNanos;
    }

    public void addObserver(LooperObserver observer) {
        if (!isAlive) {
            onStart();
        }
        synchronized (observers) {
            observers.add(observer);
        }
    }

    public void removeObserver(LooperObserver observer) {
        synchronized (observers) {
            observers.remove(observer);
            if (observers.isEmpty()) {
                onStop();
            }
        }
    }

    private void dispatchBegin() {
        token = dispatchTimeMs[0] = System.nanoTime();
        dispatchTimeMs[2] = SystemClock.currentThreadTimeMillis();
        AppMethodBeat.i(AppMethodBeat.METHOD_ID_DISPATCH);

        synchronized (observers) {
            for (LooperObserver observer : observers) {
                if (!observer.isDispatchBegin()) {
                    observer.dispatchBegin(dispatchTimeMs[0], dispatchTimeMs[2], token);
                }
            }
        }
        if (config.isDevEnv()) {
            MatrixLog.d(TAG, "[dispatchBegin#run] inner cost:%sns", System.nanoTime() - token);
        }
    }

    private void doFrameBegin(long token) {
        this.isVsyncFrame = true;
    }

    private void doFrameEnd(long token) {

        doQueueEnd(CALLBACK_TRAVERSAL);

        for (int i : queueStatus) {
            if (i != DO_QUEUE_END) {
                queueCost[i] = DO_QUEUE_END_ERROR;
                if (config.isDevEnv) {
                    throw new RuntimeException(String.format("UIThreadMonitor happens type[%s] != DO_QUEUE_END", i));
                }
            }
        }
        queueStatus = new int[CALLBACK_LAST + 1];

        addFrameCallback(CALLBACK_INPUT, this, true);

    }

    private void dispatchEnd() {
        long traceBegin = 0;
        if (config.isDevEnv()) {
            traceBegin = System.nanoTime();
        }
        long startNs = token;
        long intendedFrameTimeNs = startNs;
        if (isVsyncFrame) {
            doFrameEnd(token);
            intendedFrameTimeNs = getIntendedFrameTimeNs(startNs);
        }

        long endNs = System.nanoTime();

        synchronized (observers) {
            for (LooperObserver observer : observers) {
                if (observer.isDispatchBegin()) {
                    observer.doFrame(AppMethodBeat.getVisibleScene(), startNs, endNs, isVsyncFrame, intendedFrameTimeNs, queueCost[CALLBACK_INPUT], queueCost[CALLBACK_ANIMATION], queueCost[CALLBACK_TRAVERSAL]);
                }
            }
        }

        dispatchTimeMs[3] = SystemClock.currentThreadTimeMillis();
        dispatchTimeMs[1] = System.nanoTime();

        AppMethodBeat.o(AppMethodBeat.METHOD_ID_DISPATCH);

        synchronized (observers) {
            for (LooperObserver observer : observers) {
                if (observer.isDispatchBegin()) {
                    observer.dispatchEnd(dispatchTimeMs[0], dispatchTimeMs[2], dispatchTimeMs[1], dispatchTimeMs[3], token, isVsyncFrame);
                }
            }
        }
        this.isVsyncFrame = false;

        if (config.isDevEnv()) {
            MatrixLog.d(TAG, "[dispatchEnd#run] inner cost:%sns", System.nanoTime() - traceBegin);
        }
    }

    private void doQueueBegin(int type) {
        queueStatus[type] = DO_QUEUE_BEGIN;
        queueCost[type] = System.nanoTime();
    }

    private void doQueueEnd(int type) {
        queueStatus[type] = DO_QUEUE_END;
        queueCost[type] = System.nanoTime() - queueCost[type];
        synchronized (this) {
            callbackExist[type] = false;
        }
    }

    @Override
    public synchronized void onStart() {
        if (!isInit) {
            MatrixLog.e(TAG, "[onStart] is never init.");
            return;
        }
        if (!isAlive) {
            this.isAlive = true;
            synchronized (this) {
                MatrixLog.i(TAG, "[onStart] callbackExist:%s %s", Arrays.toString(callbackExist), Utils.getStack());
                callbackExist = new boolean[CALLBACK_LAST + 1];
            }
            queueStatus = new int[CALLBACK_LAST + 1];
            queueCost = new long[CALLBACK_LAST + 1];
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
                MatrixLog.d(TAG, "[UIThreadMonitor#run] inner cost:%sns", System.nanoTime() - start);
            }
        }
    }


    @Override
    public synchronized void onStop() {
        if (!isInit) {
            MatrixLog.e(TAG, "[onStart] is never init.");
            return;
        }
        if (isAlive) {
            this.isAlive = false;
            MatrixLog.i(TAG, "[onStop] callbackExist:%s %s", Arrays.toString(callbackExist), Utils.getStack());
        }
    }

    @Override
    public boolean isAlive() {
        return isAlive;
    }

    private long getIntendedFrameTimeNs(long defaultValue) {
        try {
            return ReflectUtils.reflectObject(vsyncReceiver, "mTimestampNanos", defaultValue);
        } catch (Exception e) {
            e.printStackTrace();
            MatrixLog.e(TAG, e.toString());
        }
        return defaultValue;
    }

    public long getQueueCost(int type, long token) {
        if (token != this.token) {
            return -1;
        }
        return queueStatus[type] == DO_QUEUE_END ? queueCost[type] : 0;
    }

    private long[] frameInfo = null;

    public long getInputEventCost() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Object obj = ReflectUtils.reflectObject(choreographer, "mFrameInfo", null);
            if (null == frameInfo) {
                frameInfo = ReflectUtils.reflectObject(obj, "frameInfo", null);
                if (null == frameInfo) {
                    frameInfo = ReflectUtils.reflectObject(obj, "mFrameInfo", new long[9]);
                }
            }
            long start = frameInfo[OLDEST_INPUT_EVENT];
            long end = frameInfo[NEWEST_INPUT_EVENT];
            return end - start;
        }
        return 0;
    }
}
