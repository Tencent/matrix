/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
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

import android.app.Activity;
import android.support.v4.app.Fragment;
import android.view.Choreographer;

import com.tencent.matrix.trace.listeners.IFrameBeat;
import com.tencent.matrix.trace.listeners.IFrameBeatListener;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.util.LinkedList;

/**
 * Created by caichongyang on 17/5/18.
 * <p>
 * The FrameBeat receives the frame event from Choreographer{@link Choreographer},
 * and it notifies tracer to deal with their own work.
 * It can help us to calculate FPS and know if mainThread does too much work.
 * </p>
 * <p>
 * The FrameBeat only cares about the main thread.
 * when the app is staying at background,it's unavailable.
 * </p>
 */

public final class FrameBeat implements IFrameBeat, Choreographer.FrameCallback, ApplicationLifeObserver.IObserver {

    private static final String TAG = "Matrix.FrameBeat";
    private static FrameBeat mInstance;
    private final LinkedList<IFrameBeatListener> mFrameListeners;
    private Choreographer mChoreographer;
    private boolean isCreated;
    private volatile boolean isPause = true;
    private long mLastFrameNanos;

    private FrameBeat() {
        mFrameListeners = new LinkedList<>();
    }


    public static FrameBeat getInstance() {
        if (null == mInstance) {
            mInstance = new FrameBeat();
        }
        return mInstance;
    }

    public boolean isPause() {
        return isPause;
    }

    public void pause() {
        if (!isCreated) {
            return;
        }
        isPause = true;
        if (null != mChoreographer) {
            mChoreographer.removeFrameCallback(this);
            for (IFrameBeatListener listener : mFrameListeners) {
                listener.cancelFrame();
            }
        }

    }

    public void resume() {
        if (!isCreated) {
            return;
        }
        isPause = false;
        if (null != mChoreographer) {
            mChoreographer.removeFrameCallback(this);
            mChoreographer.postFrameCallback(this);
        }
    }

    @Override
    public void onCreate() {
        if (!MatrixUtil.isInMainThread(Thread.currentThread().getId())) {
            MatrixLog.e(TAG, "[onCreate] FrameBeat must create on main thread");
            return;
        }
        MatrixLog.i(TAG, "[onCreate] FrameBeat real onCreate!");
        if (!isCreated) {
            isCreated = true;
            mLastFrameNanos = System.nanoTime();
            ApplicationLifeObserver.getInstance().register(this);
            mChoreographer = Choreographer.getInstance();
        } else {
            MatrixLog.w(TAG, "[onCreate] FrameBeat is created!");
        }
    }


    @Override
    public void onDestroy() {
        if (isCreated) {
            isCreated = false;
            if (null != mChoreographer) {
                mChoreographer.removeFrameCallback(this);
                for (IFrameBeatListener listener : mFrameListeners) {
                    listener.cancelFrame();
                }
            }
            mChoreographer = null;
            if (null != mFrameListeners) {
                mFrameListeners.clear();
            }
            ApplicationLifeObserver.getInstance().unregister(this);
        } else {
            MatrixLog.w(TAG, "[onDestroy] FrameBeat is not created!");
        }

    }

    @Override
    public void addListener(IFrameBeatListener listener) {
        if (null != mFrameListeners && !mFrameListeners.contains(listener)) {
            mFrameListeners.add(listener);
        }
    }

    @Override
    public void removeListener(IFrameBeatListener listener) {
        if (null != mFrameListeners) {
            mFrameListeners.remove(listener);
        }
    }

    /**
     * when the device's Vsync is coming,it will be called.
     *
     * @param frameTimeNanos The time in nanoseconds when the frame started being rendered.
     */
    @Override
    public void doFrame(long frameTimeNanos) {
        if (isPause) {
            return;
        }
        if (frameTimeNanos < mLastFrameNanos) {
            MatrixLog.w(TAG, "frameTimeNanos < mLastFrameNanos, just return");
            mLastFrameNanos = frameTimeNanos;
            if (null != mChoreographer) {
                mChoreographer.postFrameCallback(this);
            }
            return;
        }
        if (null != mFrameListeners) {
//            long start = System.currentTimeMillis();

            for (IFrameBeatListener listener : mFrameListeners) {
                listener.doFrame(mLastFrameNanos, frameTimeNanos);
            }

            if (null != mChoreographer) {
                mChoreographer.postFrameCallback(this);
            }

            mLastFrameNanos = frameTimeNanos;

//            long cost = System.currentTimeMillis() - start;
//            if (cost > 16) {
//                MatrixLog.d(TAG, "[doFrame] don't do too much work in this function!!! cost:%sms", cost);
//            }
        }

    }


    @Override
    public void onFront(Activity activity) {
        MatrixLog.i(TAG, "[onFront] isCreated:%s postFrameCallback", isCreated);
        mLastFrameNanos = System.nanoTime();
        resume();
    }

    @Override
    public void onBackground(Activity activity) {
        MatrixLog.i(TAG, "[onBackground] isCreated:%s removeFrameCallback", isCreated);
        pause();
    }

    @Override
    public void onChange(Activity activity, Fragment fragment) {
        MatrixLog.i(TAG, "[onChange] resetIndex mLastFrameNanos, current activity:%s", activity.getClass().getSimpleName());
    }

    @Override
    public void onActivityCreated(Activity activity) {

    }

    @Override
    public void onActivityPause(Activity activity) {

    }

    @Override
    public void onActivityResume(Activity activity) {

    }

    @Override
    public void onActivityStarted(Activity activity) {

    }
}
