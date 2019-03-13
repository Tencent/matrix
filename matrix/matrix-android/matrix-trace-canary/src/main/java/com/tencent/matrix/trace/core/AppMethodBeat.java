package com.tencent.matrix.trace.core;

import android.app.Activity;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;

import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.trace.listeners.IAppMethodBeatListener;
import com.tencent.matrix.trace.util.Utils;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashSet;
import java.util.Set;

public class AppMethodBeat implements BeatLifecycle {

    private static final String TAG = "Matrix.AppMethodBeat";
    private static AppMethodBeat sInstance = new AppMethodBeat();
    private static volatile boolean isAlive = false;
    private static long[] sBuffer = new long[Constants.BUFFER_SIZE];
    private static int sIndex = 0;
    private static int sLastIndex = -1;
    private static volatile boolean isRealTrace = false;
    private static boolean assertIn = false;
    private volatile static long sCurrentDiffTime = SystemClock.uptimeMillis();
    private volatile static long sLastDiffTime = sCurrentDiffTime;
    private static Thread sMainThread = Looper.getMainLooper().getThread();
    private static HandlerThread sTimerUpdateThread = MatrixHandlerThread.getNewHandlerThread("matrix_time_update_thread");
    private static Handler sTimeUpdateHandler = new Handler(sTimerUpdateThread.getLooper());
    private static final int METHOD_ID_MAX = 0xFFFFF;
    public static final int METHOD_ID_DISPATCH = METHOD_ID_MAX - 1;
    private static Set<String> sFocusActivitySet = new HashSet<>();
    private static String sFocusedActivity = "default";
    private static HashSet<IAppMethodBeatListener> listeners = new HashSet<>();

    static {
        sTimeUpdateHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                realRelease();
            }
        }, Constants.DEFAULT_RELEASE_BUFFER_DELAY);
    }

    private static Runnable sUpdateDiffTimeRunnable = new Runnable() {
        @Override
        public void run() {
            long currentTime = SystemClock.uptimeMillis();
            sCurrentDiffTime = currentTime - sLastDiffTime;
            if (isAlive) {
                sTimeUpdateHandler.postDelayed(this, Constants.TIME_UPDATE_CYCLE_MS);
            }
        }
    };

    public static AppMethodBeat getInstance() {
        return sInstance;
    }

    @Override
    public void onStart() {
        if (!isAlive) {
            assert sBuffer != null;
            this.isAlive = true;
            updateDiffTime();
            MatrixLog.i(TAG, "[onStart] %s", Utils.getStack());
        }
    }

    @Override
    public void onStop() {
        if (isAlive) {
            MatrixLog.i(TAG, "[onStop] %s", Utils.getStack());
            this.isAlive = false;
        }
    }

    private static void realRelease() {
        if (!isRealTrace) {
            MatrixLog.i(TAG, "[realRelease] timestamp:%s", System.currentTimeMillis());
            sTimeUpdateHandler.removeCallbacksAndMessages(null);
            sTimerUpdateThread.quit();
            sBuffer = null;
        }
    }

    private static void realExecute() {
        MatrixLog.i(TAG, "[realExecute] timestamp:%s", System.currentTimeMillis());
        updateDiffTime();
        ActivityThreadHacker.hackSysHandlerCallback();
    }

    private static void updateDiffTime() {
        sTimeUpdateHandler.removeCallbacksAndMessages(null);
        sUpdateDiffTimeRunnable.run();
        sTimeUpdateHandler.postDelayed(sUpdateDiffTimeRunnable, Constants.TIME_UPDATE_CYCLE_MS);
    }


    /**
     * hook method when it's called in.
     *
     * @param methodId
     */
    public static void i(int methodId) {
        if (methodId >= METHOD_ID_MAX) {
            return;
        }
        if (!isRealTrace) {
            realExecute();
        }

        isRealTrace = true;

        if (!isAlive) {
            return;
        }

        if (Thread.currentThread() == sMainThread) {
            if (assertIn) {
                android.util.Log.e(TAG, "ERROR!!! AppMethodBeat.i Recursive calls!!!");
                return;
            }
            assertIn = true;
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, true);
            } else {
                sIndex = -1;
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
        if (!isAlive) {
            return;
        }
        if (methodId >= METHOD_ID_MAX) {
            return;
        }
        if (Thread.currentThread() == sMainThread) {
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, false);
            } else {
                sIndex = -1;
            }
            ++sIndex;
        }
    }

    /**
     * when the special method calls,it's will be called.
     *
     * @param activity now at which activity
     * @param isFocus  this window if has focus
     */
    public static void at(Activity activity, boolean isFocus) {
        String activityName = activity.getClass().getName();
        if (isFocus) {
            sFocusedActivity = activityName;
            if (!sFocusActivitySet.add(activityName)) {
                MatrixLog.w(TAG, "[at] maybe wrong! why has two same focused activity[%s]!", activityName);
            }
            synchronized (listeners) {
                for (IAppMethodBeatListener listener : listeners) {
                    listener.onActivityFocused(activityName);
                }
            }
        } else {
            if (sFocusedActivity.equals(activityName)) {
                sFocusedActivity = "default";
            }
            sFocusActivitySet.remove(activityName);
        }

        MatrixLog.i(TAG, "[at] Activity[%s] has %s focus!", activityName, isFocus ? "attach" : "detach");
    }


    public static String getFocusedActivity() {
        return sFocusedActivity;
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
        checkPileup(index);
        sLastIndex = index;
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
                sIndexRecordHead = indexRecord = indexRecord.next;
                MatrixLog.w(TAG, "[checkPileup] index:%s", index);
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

    public long[] copyData(IndexRecord startRecord, IndexRecord endRecord) {

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
                    length = 1 + start + (sBuffer.length - end);
                    data = new long[length];
                    System.arraycopy(sBuffer, start, data, 0, start + 1);
                    System.arraycopy(sBuffer, 0, data, start + 1, end + 1);
                }
                return data;
            }
            return data;
        } finally {
            MatrixLog.i(TAG, "[copyData] [%s:%s] cost:%sms", Math.max(0, startRecord.index), endRecord.index, System.currentTimeMillis() - current);
        }
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
