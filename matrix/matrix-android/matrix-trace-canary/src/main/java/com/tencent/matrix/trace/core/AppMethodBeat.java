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

import android.app.Activity;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;

import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner;
import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.trace.listeners.IAppMethodBeatListener;
import com.tencent.matrix.trace.listeners.ILooperListener;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashSet;
import java.util.Set;

public class AppMethodBeat implements BeatLifecycle {

    public interface MethodEnterListener {
        void enter(int method, long threadId);
    }

    private static final String TAG = "Matrix.AppMethodBeat";
    public static boolean isDev = false;
    private static AppMethodBeat sInstance = new AppMethodBeat();
    private static final int STATUS_DEFAULT = Integer.MAX_VALUE;
    private static final int STATUS_STARTED = 2;
    private static final int STATUS_READY = 1;
    private static final int STATUS_STOPPED = -1;
    private static final int STATUS_EXPIRED_START = -2;
    private static final int STATUS_OUT_RELEASE = -3;

    private static volatile int status = STATUS_DEFAULT;
    private final static Object statusLock = new Object();
    public static MethodEnterListener sMethodEnterListener;
    private static long[] sBuffer = new long[Constants.BUFFER_SIZE];
    private static int sIndex = 0;
    private static int sLastIndex = -1;
    private static boolean assertIn = false;
    private volatile static long sCurrentDiffTime = SystemClock.uptimeMillis();
    private volatile static long sDiffTime = sCurrentDiffTime;
    private static long sMainThreadId = Looper.getMainLooper().getThread().getId();
    private static HandlerThread sTimerUpdateThread = MatrixHandlerThread.getNewHandlerThread("matrix_time_update_thread", Thread.MIN_PRIORITY + 2);
    private static Handler sHandler = new Handler(sTimerUpdateThread.getLooper());
    private static final int METHOD_ID_MAX = 0xFFFFF;
    public static final int METHOD_ID_DISPATCH = METHOD_ID_MAX - 1;
    private static Set<String> sFocusActivitySet = new HashSet<>();
    private static final HashSet<IAppMethodBeatListener> listeners = new HashSet<>();
    private static final Object updateTimeLock = new Object();
    private static volatile boolean isPauseUpdateTime = false;
    private static Runnable checkStartExpiredRunnable = null;
    private static ILooperListener looperMonitorListener = new ILooperListener() {
        @Override
        public boolean isValid() {
            return status >= STATUS_READY;
        }

        @Override
        public void onDispatchBegin(String log) {
            AppMethodBeat.dispatchBegin();
        }

        @Override
        public void onDispatchEnd(String log, long beginNs, long endNs) {
            AppMethodBeat.dispatchEnd();
        }
    };
    private static Runnable realReleaseRunnable = new Runnable() {
        @Override
        public void run() {
            realRelease();
        }
    };

    static {
        MatrixHandlerThread.getDefaultHandler().postDelayed(realReleaseRunnable, Constants.DEFAULT_RELEASE_BUFFER_DELAY);
    }

    /**
     * update time runnable
     */
    private static Runnable sUpdateDiffTimeRunnable = new Runnable() {
        @Override
        public void run() {
            try {
                while (true) {
                    while (!isPauseUpdateTime && status > STATUS_STOPPED) {
                        sCurrentDiffTime = SystemClock.uptimeMillis() - sDiffTime;
                        SystemClock.sleep(Constants.TIME_UPDATE_CYCLE_MS);
                    }
                    synchronized (updateTimeLock) {
                        updateTimeLock.wait();
                    }
                }
            } catch (Exception e) {
                MatrixLog.e(TAG, "" + e.toString());
            }
        }
    };

    public static AppMethodBeat getInstance() {
        return sInstance;
    }

    @Override
    public void onStart() {
        synchronized (statusLock) {
            if (status < STATUS_STARTED && status >= STATUS_EXPIRED_START) {
                sHandler.removeCallbacks(checkStartExpiredRunnable);
                MatrixHandlerThread.getDefaultHandler().removeCallbacks(realReleaseRunnable);
                if (sBuffer == null) {
                    throw new RuntimeException(TAG + " sBuffer == null");
                }
                MatrixLog.i(TAG, "[onStart] preStatus:%s", status, Utils.getStack());
                status = STATUS_STARTED;
            } else {
                MatrixLog.w(TAG, "[onStart] current status:%s", status);
            }
        }
    }

    @Override
    public void onStop() {
        synchronized (statusLock) {
            if (status == STATUS_STARTED) {
                MatrixLog.i(TAG, "[onStop] %s", Utils.getStack());
                status = STATUS_STOPPED;
            } else {
                MatrixLog.w(TAG, "[onStop] current status:%s", status);
            }
        }
    }

    public void forceStop() {
        synchronized (statusLock) {
            status = STATUS_STOPPED;
        }
    }

    @Override
    public boolean isAlive() {
        return status >= STATUS_STARTED;
    }


    public static boolean isRealTrace() {
        return status >= STATUS_READY;
    }

    private static void realRelease() {
        synchronized (statusLock) {
            if (status == STATUS_DEFAULT || status <= STATUS_READY) {
                MatrixLog.i(TAG, "[realRelease] timestamp:%s", System.currentTimeMillis());
                sHandler.removeCallbacksAndMessages(null);
                LooperMonitor.unregister(looperMonitorListener);
                sTimerUpdateThread.quit();
                sBuffer = null;
                status = STATUS_OUT_RELEASE;
            }
        }
    }

    private static void realExecute() {
        MatrixLog.i(TAG, "[realExecute] timestamp:%s", System.currentTimeMillis());
        sCurrentDiffTime = SystemClock.uptimeMillis() - sDiffTime;

        sHandler.removeCallbacksAndMessages(null);
        sHandler.postDelayed(sUpdateDiffTimeRunnable, Constants.TIME_UPDATE_CYCLE_MS);
        sHandler.postDelayed(checkStartExpiredRunnable = new Runnable() {
            @Override
            public void run() {
                synchronized (statusLock) {
                    MatrixLog.i(TAG, "[startExpired] timestamp:%s status:%s", System.currentTimeMillis(), status);
                    if (status == STATUS_DEFAULT || status == STATUS_READY) {
                        status = STATUS_EXPIRED_START;
                    }
                }
            }
        }, Constants.DEFAULT_RELEASE_BUFFER_DELAY);

        ActivityThreadHacker.hackSysHandlerCallback();
        LooperMonitor.register(looperMonitorListener);
    }

    private static void dispatchBegin() {
        sCurrentDiffTime = SystemClock.uptimeMillis() - sDiffTime;
        isPauseUpdateTime = false;

        synchronized (updateTimeLock) {
            updateTimeLock.notify();
        }
    }

    private static void dispatchEnd() {
        isPauseUpdateTime = true;
    }

    /**
     * hook method when it's called in.
     *
     * @param methodId
     */
    public static void i(int methodId) {

        if (status <= STATUS_STOPPED) {
            return;
        }
        if (methodId >= METHOD_ID_MAX) {
            return;
        }

        if (status == STATUS_DEFAULT) {
            synchronized (statusLock) {
                if (status == STATUS_DEFAULT) {
                    realExecute();
                    status = STATUS_READY;
                }
            }
        }

        long threadId = Thread.currentThread().getId();
        if (sMethodEnterListener != null) {
            sMethodEnterListener.enter(methodId, threadId);
        }

        if (threadId == sMainThreadId) {
            if (assertIn) {
                android.util.Log.e(TAG, "ERROR!!! AppMethodBeat.i Recursive calls!!!");
                return;
            }
            assertIn = true;
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, true);
            } else {
                sIndex = 0;
                 mergeData(methodId, sIndex, true);
            }
            ++sIndex;
            assertIn = false;
        }
    }

    /**
     * hook method when it's called out.
     *
     * @param methodId
     */
    public static void o(int methodId) {
        if (status <= STATUS_STOPPED) {
            return;
        }
        if (methodId >= METHOD_ID_MAX) {
            return;
        }
        if (Thread.currentThread().getId() == sMainThreadId) {
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, false);
            } else {
                sIndex = 0;
                mergeData(methodId, sIndex, false);
            }
            ++sIndex;
        }
    }

    /**
     * when the special method calls,it's will be called.
     *
     * called after {@link #i(int)}
     *
     * @param activity now at which activity
     * @param isFocus  this window if has focus
     */
    public static void at(Activity activity, boolean isFocus) {
        String activityName = activity.getClass().getName();
        if (isFocus) {
            if (sFocusActivitySet.add(activityName)) {
                synchronized (listeners) {
                    for (IAppMethodBeatListener listener : listeners) {
                        listener.onActivityFocused(activity);
                    }
                }
                MatrixLog.i(TAG, "[at] visibleScene[%s] has %s focus!", getVisibleScene(), "attach");
            }
        } else {
            if (sFocusActivitySet.remove(activityName)) {
                MatrixLog.i(TAG, "[at] visibleScene[%s] has %s focus!", getVisibleScene(), "detach");
            }
        }
    }

    public static String getVisibleScene() {
        return ProcessUILifecycleOwner.INSTANCE.getVisibleScene();
    }

    /**
     * merge trace info as a long data
     *
     * @param methodId
     * @param index
     * @param isIn
     */
    private static void mergeData(int methodId, int index, boolean isIn) {
        if (methodId == AppMethodBeat.METHOD_ID_DISPATCH) {
            sCurrentDiffTime = SystemClock.uptimeMillis() - sDiffTime;
        }

        try {
            long trueId = 0L;
            if (isIn) {
                trueId |= 1L << 63;
            }
            trueId |= (long) methodId << 43;
            trueId |= sCurrentDiffTime & 0x7FFFFFFFFFFL;
            sBuffer[index] = trueId;
            checkPileup(index);
            sLastIndex = index;
        } catch (Throwable t) {
            MatrixLog.e(TAG, t.getMessage());
        }
    }

    public void addListener(IAppMethodBeatListener listener) {
        synchronized (listeners) {
            listeners.add(listener);
        }
    }

    public void removeListener(IAppMethodBeatListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    private static IndexRecord sIndexRecordHead = null;

    public IndexRecord maskIndex(String source) {
        if (sIndexRecordHead == null) {
            sIndexRecordHead = new IndexRecord(sIndex - 1);
            sIndexRecordHead.source = source;
            return sIndexRecordHead;
        } else {
            IndexRecord indexRecord = new IndexRecord(sIndex - 1);
            indexRecord.source = source;
            IndexRecord record = sIndexRecordHead;
            IndexRecord last = null;

            while (record != null) {
                if (indexRecord.index <= record.index) {
                    if (null == last) {
                        IndexRecord tmp = sIndexRecordHead;
                        sIndexRecordHead = indexRecord;
                        indexRecord.next = tmp;
                    } else {
                        IndexRecord tmp = last.next;
                        last.next = indexRecord;
                        indexRecord.next = tmp;
                    }
                    return indexRecord;
                }
                last = record;
                record = record.next;
            }
            last.next = indexRecord;

            return indexRecord;
        }
    }

    private static void checkPileup(int index) {
        IndexRecord indexRecord = sIndexRecordHead;
        while (indexRecord != null) {
            if (indexRecord.index == index || (indexRecord.index == -1 && sLastIndex == Constants.BUFFER_SIZE - 1)) {
                indexRecord.isValid = false;
                MatrixLog.w(TAG, "[checkPileup] %s", indexRecord.toString());
                sIndexRecordHead = indexRecord = indexRecord.next;
            } else {
                break;
            }
        }
    }

    public static final class IndexRecord {
        public IndexRecord(int index) {
            this.index = index;
        }

        public IndexRecord() {
            this.isValid = false;
        }

        public int index;
        private IndexRecord next;
        public boolean isValid = true;
        public String source;

        public void release() {
            isValid = false;
            IndexRecord record = sIndexRecordHead;
            IndexRecord last = null;
            while (null != record) {
                if (record == this) {
                    if (null != last) {
                        last.next = record.next;
                    } else {
                        sIndexRecordHead = record.next;
                    }
                    record.next = null;
                    break;
                }
                last = record;
                record = record.next;
            }
        }

        @Override
        public String toString() {
            return "index:" + index + ",\tisValid:" + isValid + " source:" + source;
        }
    }

    public long[] copyData(IndexRecord startRecord) {
        return copyData(startRecord, new IndexRecord(sIndex - 1));
    }

    private long[] copyData(IndexRecord startRecord, IndexRecord endRecord) {
        long current = System.currentTimeMillis();
        long[] data = new long[0];
        try {
            if (startRecord.isValid && endRecord.isValid) {
                int length;
                int start = Math.max(0, startRecord.index);
                int end = Math.max(0, endRecord.index);

                if (end > start) {
                    length = end - start + 1;
                    data = new long[length];
                    System.arraycopy(sBuffer, start, data, 0, length);
                } else if (end < start) {
                    length = 1 + end + (sBuffer.length - start);
                    data = new long[length];
                    System.arraycopy(sBuffer, start, data, 0, sBuffer.length - start);
                    System.arraycopy(sBuffer, 0, data, sBuffer.length - start, end + 1);
                }
                return data;
            }
            return data;
        } catch (Throwable t) {
            MatrixLog.e(TAG, t.toString());
            return data;
        } finally {
            MatrixLog.i(TAG, "[copyData] [%s:%s] length:%s cost:%sms", Math.max(0, startRecord.index), endRecord.index, data.length, System.currentTimeMillis() - current);
        }
    }

    public static long getDiffTime() {
        return sDiffTime;
    }

    public void printIndexRecord() {
        StringBuilder ss = new StringBuilder(" \n");
        IndexRecord record = sIndexRecordHead;
        while (null != record) {
            ss.append(record).append("\n");
            record = record.next;
        }
        MatrixLog.i(TAG, "[printIndexRecord] %s", ss.toString());
    }

}
