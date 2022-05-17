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
import android.os.Looper;
import android.os.SystemClock;
import android.view.Choreographer;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.listeners.LooperObserver;
import com.tencent.matrix.trace.util.Utils;
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
     * @hide
     */
    public static final int CALLBACK_INPUT_16_22 = 0;

    /**
     * Callback type: Animation callback.  Runs before traversals.
     * @hide
     */
    public static final int CALLBACK_ANIMATION_16_22 = 1;

    /**
     * Callback type: Traversal callback.  Handles layout and draw.  Runs last
     * after all other asynchronous messages have been handled.
     * @hide
     */
    public static final int CALLBACK_TRAVERSAL_16_22 = 2;

    /**
     * Callback type: Input callback.  Runs first.
     * @hide
     */
    public static final int CALLBACK_INPUT_23_28 = 0;

    /**
     * Callback type: Animation callback.  Runs before traversals.
     * @hide
     */
    public static final int CALLBACK_ANIMATION_23_28 = 1;

    /**
     * Callback type: Traversal callback.  Handles layout and draw.  Runs
     * after all other asynchronous messages have been handled.
     * @hide
     */
    public static final int CALLBACK_TRAVERSAL_23_28 = 2;

    /**
     * Callback type: Commit callback.  Handles post-draw operations for the frame.
     * Runs after traversal completes.  The {@link #getFrameTime() frame time} reported
     * during this callback may be updated to reflect delays that occurred while
     * traversals were in progress in case heavy layout operations caused some frames
     * to be skipped.  The frame time reported during this callback provides a better
     * estimate of the start time of the frame in which animations (and other updates
     * to the view hierarchy state) actually took effect.
     * @hide
     */
    public static final int CALLBACK_COMMIT_23_28 = 3;

    /**
     * Callback type: Input callback.  Runs first.
     * @hide
     */
    public static final int CALLBACK_INPUT_29_ = 0;

    /**
     * Callback type: Animation callback.  Runs before {@link #CALLBACK_INSETS_ANIMATION}.
     * @hide
     */
    public static final int CALLBACK_ANIMATION_29_ = 1;

    /**
     * Callback type: Animation callback to handle inset updates. This is separate from
     * {@link #CALLBACK_ANIMATION} as we need to "gather" all inset animation updates via
     * {@link WindowInsetsAnimationController#changeInsets} for multiple ongoing animations but then
     * update the whole view system with a single callback to {@link View#dispatchWindowInsetsAnimationProgress}
     * that contains all the combined updated insets.
     * <p>
     * Both input and animation may change insets, so we need to run this after these callbacks, but
     * before traversals.
     * <p>
     * Runs before traversals.
     * @hide
     */
    public static final int CALLBACK_INSETS_ANIMATION_29_ = 2;

    /**
     * Callback type: Traversal callback.  Handles layout and draw.  Runs
     * after all other asynchronous messages have been handled.
     * @hide
     */
    public static final int CALLBACK_TRAVERSAL_29_ = 3;

    /**
     * Callback type: Commit callback.  Handles post-draw operations for the frame.
     * Runs after traversal completes.  The {@link #getFrameTime() frame time} reported
     * during this callback may be updated to reflect delays that occurred while
     * traversals were in progress in case heavy layout operations caused some frames
     * to be skipped.  The frame time reported during this callback provides a better
     * estimate of the start time of the frame in which animations (and other updates
     * to the view hierarchy state) actually took effect.
     * @hide
     */
    public static final int CALLBACK_COMMIT_29_ = 4;


    /**
     * never do queue end code
     */
    public static final int DO_QUEUE_END_ERROR = -100;

    private static int getCallbackFirst() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            return CALLBACK_INPUT_16_22;
        } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            return CALLBACK_INPUT_23_28;
        } else {
            return CALLBACK_INPUT_29_;
        }
    }

    private static int getCallbackLast() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
            return CALLBACK_TRAVERSAL_16_22;
        } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            return CALLBACK_COMMIT_23_28;
        } else {
            return CALLBACK_COMMIT_29_;
        }
    }

    private static final int CALLBACK_FIRST = getCallbackFirst();
    private static final int CALLBACK_LAST = getCallbackLast();

    private final static UIThreadMonitor sInstance = new UIThreadMonitor();
    private TraceConfig config;
    private static boolean useFrameMetrics;
    private Object callbackQueueLock;
    private Object[] callbackQueues;
    private Method addInputQueue;
    private Method addAnimationQueue;
    private Method addInsetsAnimationQueue;
    private Method addTraversalQueue;
    private Method addCommitQueue;
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

    public void init(TraceConfig config, boolean supportFrameMetrics) {
        this.config = config;
        useFrameMetrics = supportFrameMetrics;

        if (Thread.currentThread() != Looper.getMainLooper().getThread()) {
            throw new AssertionError("must be init in main thread!");
        }

        boolean historyMsgRecorder = config.historyMsgRecorder;
        boolean denseMsgTracer = config.denseMsgTracer;

        LooperMonitor.register(new LooperMonitor.LooperDispatchListener(historyMsgRecorder, denseMsgTracer) {
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
        frameIntervalNanos = ReflectUtils.reflectObject(choreographer, "mFrameIntervalNanos", Constants.DEFAULT_FRAME_DURATION);
        if (!useFrameMetrics) {
            choreographer = Choreographer.getInstance();
            callbackQueueLock = ReflectUtils.reflectObject(choreographer, "mLock", new Object());
            callbackQueues = ReflectUtils.reflectObject(choreographer, "mCallbackQueues", null);
            if (null != callbackQueues) {
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                    addInputQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_INPUT_16_22], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addAnimationQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_ANIMATION_16_22], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addTraversalQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_TRAVERSAL_16_22], ADD_CALLBACK, long.class, Object.class, Object.class);
                } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                    addInputQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_INPUT_23_28], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addAnimationQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_ANIMATION_23_28], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addTraversalQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_TRAVERSAL_23_28], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addCommitQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_COMMIT_23_28], ADD_CALLBACK, long.class, Object.class, Object.class);
                } else {
                    addInputQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_INPUT_29_], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addAnimationQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_ANIMATION_29_], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addInsetsAnimationQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_INSETS_ANIMATION_29_], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addTraversalQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_TRAVERSAL_29_], ADD_CALLBACK, long.class, Object.class, Object.class);
                    addCommitQueue = ReflectUtils.reflectMethod(callbackQueues[CALLBACK_COMMIT_29_], ADD_CALLBACK, long.class, Object.class, Object.class);
                }

            }
            vsyncReceiver = ReflectUtils.reflectObject(choreographer, "mDisplayEventReceiver", null);

            MatrixLog.i(TAG, "[UIThreadMonitor] %s %s %s %s %s %s %s %s frameIntervalNanos:%s", callbackQueueLock == null, callbackQueues == null,
                    addInputQueue == null, addAnimationQueue == null, addInsetsAnimationQueue == null, addTraversalQueue == null, addCommitQueue == null,
                    vsyncReceiver == null, frameIntervalNanos);

            if (config.isDevEnv()) {
                addObserver(new LooperObserver() {
                    @Override
                    public void doFrame(String focusedActivity, long startNs, long endNs, boolean isVsyncFrame, long intendedFrameTimeNs,
                                        long inputCostNs, long animationCostNs, long insetsAnimationCostNs, long traversalCostNs, long commitCostNs) {
                        MatrixLog.i(TAG, "focusedActivity[%s] frame cost:%sms isVsyncFrame=%s intendedFrameTimeNs=%s [%s|%s|%s|%s|%s]ns",
                                focusedActivity, (endNs - startNs) / Constants.TIME_MILLIS_TO_NANO, isVsyncFrame, intendedFrameTimeNs,
                                inputCostNs, animationCostNs, insetsAnimationCostNs, traversalCostNs, commitCostNs);
                    }
                });
            }
        }
    }

    private synchronized void addFrameCallback(int type, Runnable callback, boolean isAddHeader) {
        if (callbackExist[type]) {
            MatrixLog.w(TAG, "[addFrameCallback] this type %s callback has exist! isAddHeader:%s", type, isAddHeader);
            return;
        }

        if (!isAlive && type == CALLBACK_FIRST) {
            MatrixLog.w(TAG, "[addFrameCallback] UIThreadMonitor is not alive!");
            return;
        }

        try {
            synchronized (callbackQueueLock) {
                Method method = null;

                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                    switch (type) {
                        case CALLBACK_INPUT_16_22:
                            method = addInputQueue;
                            break;
                        case CALLBACK_ANIMATION_16_22:
                            method = addAnimationQueue;
                            break;
                        case CALLBACK_TRAVERSAL_16_22:
                            method = addTraversalQueue;
                            break;
                    }
                } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                    switch (type) {
                        case CALLBACK_INPUT_23_28:
                            method = addInputQueue;
                            break;
                        case CALLBACK_ANIMATION_23_28:
                            method = addAnimationQueue;
                            break;
                        case CALLBACK_TRAVERSAL_23_28:
                            method = addTraversalQueue;
                            break;
                        case CALLBACK_COMMIT_23_28:
                            method = addCommitQueue;
                            break;
                    }

                } else {
                    switch (type) {
                        case CALLBACK_INPUT_29_:
                            method = addInputQueue;
                            break;
                        case CALLBACK_ANIMATION_29_:
                            method = addAnimationQueue;
                            break;
                        case CALLBACK_INSETS_ANIMATION_29_:
                            method = addInsetsAnimationQueue;
                            break;
                        case CALLBACK_TRAVERSAL_29_:
                            method = addTraversalQueue;
                            break;
                        case CALLBACK_COMMIT_29_:
                            method = addCommitQueue;
                            break;
                    }
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
        if (config.isAppMethodBeatEnable()) {
            AppMethodBeat.i(AppMethodBeat.METHOD_ID_DISPATCH);
        }
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

        doQueueEnd(CALLBACK_LAST);


        for (int i : queueStatus) {
            if (i != DO_QUEUE_END) {
                queueCost[i] = DO_QUEUE_END_ERROR;
                if (config.isDevEnv) {
                    throw new RuntimeException(String.format("UIThreadMonitor happens type[%s] != DO_QUEUE_END", i));
                }
            }
        }
        queueStatus = new int[CALLBACK_LAST + 1];

        addFrameCallback(CALLBACK_FIRST, this, true);
    }

    private void dispatchEnd() {
        long traceBegin = 0;
        if (config.isDevEnv()) {
            traceBegin = System.nanoTime();
        }

        if (config.isFPSEnable() && !useFrameMetrics) {
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
                        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                            observer.doFrame(AppActiveMatrixDelegate.INSTANCE.getVisibleScene(), startNs, endNs, isVsyncFrame, intendedFrameTimeNs,
                                    queueCost[CALLBACK_INPUT_16_22], queueCost[CALLBACK_ANIMATION_16_22], 0, queueCost[CALLBACK_TRAVERSAL_16_22], 0);

                        } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                            observer.doFrame(AppActiveMatrixDelegate.INSTANCE.getVisibleScene(), startNs, endNs, isVsyncFrame, intendedFrameTimeNs,
                                    queueCost[CALLBACK_INPUT_23_28], queueCost[CALLBACK_ANIMATION_23_28], 0, queueCost[CALLBACK_TRAVERSAL_23_28], queueCost[CALLBACK_COMMIT_23_28]);
                        } else {
                            observer.doFrame(AppActiveMatrixDelegate.INSTANCE.getVisibleScene(), startNs, endNs, isVsyncFrame, intendedFrameTimeNs,
                                    queueCost[CALLBACK_INPUT_29_], queueCost[CALLBACK_ANIMATION_29_], queueCost[CALLBACK_INSETS_ANIMATION_29_], queueCost[CALLBACK_TRAVERSAL_29_], queueCost[CALLBACK_COMMIT_29_]);
                        }
                    }
                }
            }
        }

        if (config.isEvilMethodTraceEnable() || config.isDevEnv()) {
            dispatchTimeMs[3] = SystemClock.currentThreadTimeMillis();
            dispatchTimeMs[1] = System.nanoTime();
        }

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
            if (!useFrameMetrics) {
                queueStatus = new int[CALLBACK_LAST + 1];
                queueCost = new long[CALLBACK_LAST + 1];
                addFrameCallback(CALLBACK_FIRST, this, true);
            }
        }
    }

    @Override
    public void run() {
        final long start = System.nanoTime();
        try {
            doFrameBegin(token);
            doQueueBegin(CALLBACK_FIRST);

            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                addFrameCallback(CALLBACK_ANIMATION_16_22, new Runnable() {

                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_FIRST);
                        doQueueBegin(CALLBACK_ANIMATION_16_22);
                    }
                }, true);

                addFrameCallback(CALLBACK_TRAVERSAL_16_22, new Runnable() {

                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_ANIMATION_16_22);
                        doQueueBegin(CALLBACK_TRAVERSAL_16_22);
                    }
                }, true);

            } else if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
                addFrameCallback(CALLBACK_ANIMATION_23_28, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_FIRST);
                        doQueueBegin(CALLBACK_ANIMATION_23_28);
                    }
                }, true);
                addFrameCallback(CALLBACK_TRAVERSAL_23_28, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_ANIMATION_23_28);
                        doQueueBegin(CALLBACK_TRAVERSAL_23_28);
                    }
                }, true);
                addFrameCallback(CALLBACK_COMMIT_23_28, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_TRAVERSAL_23_28);
                        doQueueBegin(CALLBACK_COMMIT_23_28);
                    }
                }, true);

            } else {
                addFrameCallback(CALLBACK_ANIMATION_29_, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_FIRST);
                        doQueueBegin(CALLBACK_ANIMATION_29_);
                    }
                }, true);
                addFrameCallback(CALLBACK_INSETS_ANIMATION_29_, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_ANIMATION_29_);
                        doQueueBegin(CALLBACK_INSETS_ANIMATION_29_);
                    }
                }, true);
                addFrameCallback(CALLBACK_TRAVERSAL_29_, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_INSETS_ANIMATION_29_);
                        doQueueBegin(CALLBACK_TRAVERSAL_29_);
                    }
                }, true);
                addFrameCallback(CALLBACK_COMMIT_29_, new Runnable() {
                    @Override
                    public void run() {
                        doQueueEnd(CALLBACK_TRAVERSAL_29_);
                        doQueueBegin(CALLBACK_COMMIT_29_);
                    }
                }, true);
            }

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
