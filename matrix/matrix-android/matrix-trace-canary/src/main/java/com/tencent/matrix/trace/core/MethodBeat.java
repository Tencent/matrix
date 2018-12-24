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
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.support.v4.app.Fragment;

import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.hacker.Hacker;
import com.tencent.matrix.trace.listeners.IMethodBeat;
import com.tencent.matrix.trace.listeners.IMethodBeatListener;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.LinkedList;

/**
 * Created by caichongyang on 2017/5/26.
 * <p>
 * The MethodBeat receives the in/out method called event,
 * and it notifies tracer to deal with their own work.
 * </p>
 * <p>
 * The MethodBeat only cares about the main thread.
 * when the app is staying at background,it's unavailable.
 * </p>
 */

public class MethodBeat implements IMethodBeat, ApplicationLifeObserver.IObserver {

    private static final String TAG = "Matrix.MethodBeat";
    private volatile static boolean isCreated = false;
    private static LinkedList<IMethodBeatListener> sListeners = new LinkedList<>();
    private volatile static int sIndex = 0;
    private volatile static long sCurrentDiffTime;
    private volatile static long sLastDiffTime;
    private static long[] sBuffer;
    private static boolean isRealTrace;
    private static boolean sIsIn = false;
    private volatile static boolean isBackground = false;
    private static Thread sMainThread = Looper.getMainLooper().getThread();
    private static final int UPDATE_TIME_MSG_ID = 0x100;
    private static final int RELEASE_BUFFER_MSG_ID = 0x200;
    private static HandlerThread sTimerUpdateThread = MatrixHandlerThread.getNewHandlerThread("matrix_time_update_thread");
    private static Handler sTimeUpdateHandler = new Handler(sTimerUpdateThread.getLooper(), new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == UPDATE_TIME_MSG_ID) {
                updateDiffTime();
                if (!isBackground) {
                    sTimeUpdateHandler.sendEmptyMessageDelayed(UPDATE_TIME_MSG_ID, Constants.TIME_UPDATE_CYCLE_MS);
                } else {
                    MatrixLog.w(TAG, "stop time update!");
                }
            }
            return true;
        }
    });


    private static Handler sReleaseBufferHandler = new Handler(Looper.getMainLooper(), new Handler.Callback() {
        @Override
        public boolean handleMessage(Message msg) {
            if (msg.what == RELEASE_BUFFER_MSG_ID) {
                if (!isCreated) {
                    MatrixLog.i(TAG, "Plugin is never init, release buffer!");
                    sBuffer = null;
                    sTimeUpdateHandler.removeCallbacksAndMessages(null);
                    isBackground = true;
                }
            }
            return true;
        }
    });

    static {
        Hacker.hackSysHandlerCallback();
        sCurrentDiffTime = sLastDiffTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO;
        sReleaseBufferHandler.sendEmptyMessageDelayed(RELEASE_BUFFER_MSG_ID, Constants.DEFAULT_RELEASE_BUFFER_DELAY);
    }

    public static long getCurrentDiffTime() {
        return sCurrentDiffTime;
    }

    private static void updateDiffTime() {
        long currentTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO;
        sCurrentDiffTime = currentTime - sLastDiffTime;
    }


    /**
     * hook method when it's called in.
     *
     * @param methodId
     */
    public static void i(int methodId) {
        if (isBackground) {
            return;
        }

        if (!isRealTrace) {
            updateDiffTime();
            sTimeUpdateHandler.sendEmptyMessage(UPDATE_TIME_MSG_ID);
            sBuffer = new long[Constants.BUFFER_TMP_SIZE];
        }
        isRealTrace = true;
        if (isCreated && Thread.currentThread() == sMainThread) {
            if (sIsIn) {
                android.util.Log.e(TAG, "ERROR!!! MethodBeat.i Recursive calls!!!");
                return;
            }

            sIsIn = true;

            if (sIndex >= Constants.BUFFER_SIZE) {
                for (IMethodBeatListener listener : sListeners) {
                    listener.pushFullBuffer(0, Constants.BUFFER_SIZE - 1, sBuffer);
                }
                sIndex = 0;
            } else {
                mergeData(methodId, sIndex, true);
            }
            ++sIndex;
            sIsIn = false;
        } else if (!isCreated && Thread.currentThread() == sMainThread && sBuffer != null) {
            if (sIsIn) {
                android.util.Log.e(TAG, "ERROR!!! MethodBeat.i Recursive calls!!!");
                return;
            }

            sIsIn = true;

            if (sIndex < Constants.BUFFER_TMP_SIZE) {
                mergeData(methodId, sIndex, true);
                ++sIndex;
            }

            sIsIn = false;
        }


    }

    /**
     * hook method when it's called out.
     *
     * @param methodId
     */
    public static void o(int methodId) {
        if (isBackground || null == sBuffer) {
            return;
        }
        if (isCreated && Thread.currentThread() == sMainThread) {
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, false);
            } else {
                for (IMethodBeatListener listener : sListeners) {
                    listener.pushFullBuffer(0, Constants.BUFFER_SIZE - 1, sBuffer);
                }
                sIndex = 0;
            }
            ++sIndex;
        } else if (!isCreated && Thread.currentThread() == sMainThread) {
            if (sIndex < Constants.BUFFER_TMP_SIZE) {
                mergeData(methodId, sIndex, false);
                ++sIndex;
            }
        }
    }

    /**
     * when the special method calls,it's will be called.
     *
     * @param activity now at which activity
     * @param isFocus  this window if has focus
     */
    public static void at(Activity activity, boolean isFocus) {
        MatrixLog.i(TAG, "[AT] activity: %s, isCreated: %b sListener size: %d，isFocus: %b",
                activity.getClass().getSimpleName(), isCreated, sListeners.size(), isFocus);
        if (isCreated && Thread.currentThread() == sMainThread) {
            for (IMethodBeatListener listener : sListeners) {
                listener.onActivityEntered(activity, isFocus, sIndex - 1, sBuffer);
            }
        }
    }

    /**
     * merge trace info as a long data
     *
     * @param methodId
     * @param index
     * @param isIn
     */
    private static void mergeData(int methodId, int index, boolean isIn) {
        long trueId = 0L;
        if (isIn) {
            trueId |= 1L << 63;
        }
        trueId |= (long) methodId << 43;
        trueId |= sCurrentDiffTime & 0x7FFFFFFFFFFL;
        sBuffer[index] = trueId;
    }

    @Override
    public void onCreate() {
        if (!isCreated) {
            sReleaseBufferHandler.removeMessages(RELEASE_BUFFER_MSG_ID);
            sTimeUpdateHandler.removeMessages(UPDATE_TIME_MSG_ID);
            sTimeUpdateHandler.sendEmptyMessage(UPDATE_TIME_MSG_ID);
            ApplicationLifeObserver.getInstance().register(this);
            isCreated = true;
            if (null != sBuffer && sBuffer.length < Constants.BUFFER_SIZE) {
                final long[] tmpBuffer = sBuffer;
                sBuffer = new long[Constants.BUFFER_SIZE];
                System.arraycopy(tmpBuffer, 0, sBuffer, 0, sIndex);
            } else {
                sBuffer = new long[Constants.BUFFER_SIZE];
            }
        }
    }

    @Override
    public void onDestroy() {
        if (isCreated) {
            MatrixLog.i(TAG, "[onDestroy]");
            sListeners.clear();
            isCreated = false;
            sIndex = 0;
            sBuffer = null;
            sTimeUpdateHandler.removeMessages(UPDATE_TIME_MSG_ID);
            sReleaseBufferHandler.removeMessages(RELEASE_BUFFER_MSG_ID);
//            sTimerUpdateThread.quit();
            ApplicationLifeObserver.getInstance().unregister(this);
        }
    }

    @Override
    public boolean isRealTrace() {
        return isRealTrace;
    }

    @Override
    public long getLastDiffTime() {
        return sLastDiffTime;
    }

    private int lockCount;
    private long mLastLockBufferTime;

    @Override
    public void lockBuffer(boolean isLock) {
        if (isLock) {
            mLastLockBufferTime = System.currentTimeMillis();
            lockCount++;
        } else {
            lockCount--;
            lockCount = Math.max(0, lockCount);
        }
    }

    @Override
    public boolean isLockBuffer() {
        long duration = System.currentTimeMillis() - mLastLockBufferTime;
        if (duration > 20 * 1000) {
            lockCount = 0;
        }
        return lockCount > 0;
    }


    @Override
    public void registerListener(IMethodBeatListener listener) {
        if (!sListeners.contains(listener)) {
            sListeners.add(listener);
        }
    }

    @Override
    public void unregisterListener(IMethodBeatListener listener) {
        if (sListeners.contains(listener)) {
            sListeners.remove(listener);
        }
    }

    @Override
    public boolean isHasListeners() {
        return !sListeners.isEmpty();
    }

    public static long[] getBuffer() {
        return sBuffer;
    }

    public static int getCurIndex() {
        return sIndex;
    }

    @Override
    public void resetIndex() {
        if (!isLockBuffer()) {
            sIndex = 0;
        }
    }

    @Override
    public void onFront(Activity activity) {
        MatrixLog.i(TAG, "[onFront]: %s", activity.getClass().getSimpleName());
        isBackground = false;
        if (!sTimeUpdateHandler.hasMessages(UPDATE_TIME_MSG_ID)) {
            sTimeUpdateHandler.sendEmptyMessage(UPDATE_TIME_MSG_ID);
        }
    }

    @Override
    public void onBackground(Activity activity) {
        MatrixLog.i(TAG, "[onBackground]: %s", activity.getClass().getSimpleName());
        sTimeUpdateHandler.removeMessages(UPDATE_TIME_MSG_ID);
        isBackground = true;
    }

    @Override
    public void onChange(Activity activity, Fragment fragment) {

    }

    @Override
    public void onActivityCreated(Activity activity) {
        MatrixLog.i(TAG, "[onActivityCreated]: %s", activity.getClass().getSimpleName());
        if (isBackground && !sTimeUpdateHandler.hasMessages(UPDATE_TIME_MSG_ID)) {
            sTimeUpdateHandler.sendEmptyMessage(UPDATE_TIME_MSG_ID);
        }

    }

    @Override
    public void onActivityStarted(Activity activity) {
        MatrixLog.i(TAG, "[onActivityStarted]: %s", activity.getClass().getSimpleName());
        if (isBackground && !sTimeUpdateHandler.hasMessages(UPDATE_TIME_MSG_ID)) {
            sTimeUpdateHandler.sendEmptyMessage(UPDATE_TIME_MSG_ID);
        }
    }

    @Override
    public void onActivityPause(Activity activity) {
        MatrixLog.i(TAG, "[onActivityStarted]: %s", activity.getClass().getSimpleName());
    }

    @Override
    public void onActivityResume(Activity activity) {
        MatrixLog.i(TAG, "[onActivityResume]: %s", activity.getClass().getSimpleName());
    }
}
