package com.tencent.matrix.trace.core;

import android.os.Looper;
import android.os.SystemClock;
import android.util.Printer;
import android.view.Choreographer;

import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashSet;

public class UIThreadMonitor implements BeatLifecycle, Runnable, Printer {

    public interface IFrameObserver {
        void doFrame(long start, long end, long frameCostNs, long inputCostNs, long animationCostNs, long traversalCostNs);
    }

    public interface ILooperObserver extends IFrameObserver {
        void dispatchBegin(long beginNs, long token);

        void dispatchEnd(long beginNs, long endMs, long token);
    }

    private static final String TAG = "Matrix.UIThreadMonitor";
    private static final String ADD_CALLBACK = "addCallbackLocked";
    private static final boolean isDebug = true;
    private volatile boolean isAlive = false;
    private boolean isHandleMessageEnd = true;
    private long[] dispatchTimeMs = new long[2];
    private HashSet<IFrameObserver> observers = new HashSet<>();
    private long token = 0L;
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
    private final Object callbackQueueLock;
    private final Object[] callbackQueues;
    private final Method addTraversalQueue;
    private final Method addInputQueue;
    private final Method addAnimationQueue;
    private final Choreographer choreographer;
    private int[] typeFlags = new int[CALLBACK_LAST + 1];
    private long[] queueCost = new long[CALLBACK_LAST + 1];
    private static final int DO_QUEUE_DEFAULT = 0;
    private static final int DO_QUEUE_BEGIN = 1;
    private static final int DO_QUEUE_END = 2;

    public static UIThreadMonitor getMonitor() {
        return sInstance;
    }

    private UIThreadMonitor() {
        assert Thread.currentThread() == Looper.getMainLooper().getThread();
        choreographer = Choreographer.getInstance();
        callbackQueueLock = reflectObject(choreographer, "mLock");
        callbackQueues = reflectObject(choreographer, "mCallbackQueues");

        addInputQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_INPUT], ADD_CALLBACK, long.class, Object.class, Object.class);
        addAnimationQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_ANIMATION], ADD_CALLBACK, long.class, Object.class, Object.class);
        addTraversalQueue = reflectChoreographerMethod(callbackQueues[CALLBACK_TRAVERSAL], ADD_CALLBACK, long.class, Object.class, Object.class);
        final Printer printer = reflectObject(Looper.getMainLooper(), "mLogging");
        Looper.getMainLooper().setMessageLogging(new Printer() {
            @Override
            public void println(String x) {
                if (null != printer) {
                    printer.println(x);
                }
                UIThreadMonitor.this.println(x);
            }
        });
        if (isDebug) {
            addObserver(new IFrameObserver() {
                @Override
                public void doFrame(long start, long end, long frameCost, long inputCost, long animationCost, long traversalCost) {
                    MatrixLog.i(TAG, "frame cost:%sns %s | %s | %s", frameCost, inputCost, animationCost, traversalCost);
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
        return typeFlags[type] == DO_QUEUE_END ? queueCost[type] : 0;
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
        token = dispatchTimeMs[0] = System.nanoTime();

        MethodBeat.i(MethodBeat.METHOD_ID_DISPATCH);

        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                if (observer instanceof ILooperObserver) {
                    ILooperObserver looperObserver = (ILooperObserver) observer;
                    looperObserver.dispatchBegin(dispatchTimeMs[0], token);
                }
            }
        }
    }

    private void doFrameBegin(long token) {
        this.isBelongFrame = true;
    }

    private void doFrameEnd(long token) {
        for (int i : typeFlags) {
            if (i != DO_QUEUE_END) {
                throw new RuntimeException(String.format("UIThreadMonitor happens type[%s] != DO_QUEUE_END", i));
            }
        }
        typeFlags = new int[CALLBACK_LAST + 1];

        long start = token;
        long end = System.nanoTime();
        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                observer.doFrame(start, end, end - start, queueCost[CALLBACK_INPUT], queueCost[CALLBACK_ANIMATION], queueCost[CALLBACK_TRAVERSAL]);
            }
        }
    }

    private void dispatchEnd() {
        if (isBelongFrame) {
            doQueueEnd(CALLBACK_TRAVERSAL);
            doFrameEnd(token);
            addFrameCallback(CALLBACK_INPUT, this, true);
        }

        this.isBelongFrame = false;
        dispatchTimeMs[1] = System.nanoTime();

        MethodBeat.o(MethodBeat.METHOD_ID_DISPATCH);

        synchronized (observers) {
            for (IFrameObserver observer : observers) {
                if (observer instanceof ILooperObserver) {
                    ILooperObserver looperObserver = (ILooperObserver) observer;
                    looperObserver.dispatchEnd(dispatchTimeMs[0], dispatchTimeMs[1], token);
                }
            }
        }

    }

    private void doQueueBegin(int type) {
        typeFlags[type] = DO_QUEUE_BEGIN;
        queueCost[type] = System.nanoTime();
    }

    private void doQueueEnd(int type) {
        typeFlags[type] = DO_QUEUE_END;
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
    public void println(String x) {
        this.isHandleMessageEnd = !isHandleMessageEnd;
        if (!isHandleMessageEnd) {
            dispatchBegin();
        } else {
            dispatchEnd();
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
            if (isDebug) {
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
