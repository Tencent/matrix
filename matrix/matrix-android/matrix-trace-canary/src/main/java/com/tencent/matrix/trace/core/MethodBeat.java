package com.tencent.matrix.trace.core;

import android.app.Activity;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import com.tencent.matrix.trace.constants.Constants;
import com.tencent.matrix.trace.hacker.ActivityThreadHacker;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;

public class MethodBeat implements BeatLifecycle {

    private static final String TAG = "Matrix.MethodBeat";
    private static MethodBeat sInstance = new MethodBeat();
    private static volatile boolean isAlive = false;
    private static long[] sBuffer = new long[Constants.BUFFER_SIZE];
    private static int sIndex = 0;
    private static int sLastIndex = -1;
    private static volatile boolean isRealTrace = false;
    private static boolean assertIn = false;
    private volatile static long sCurrentDiffTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO;
    private volatile static long sLastDiffTime = sCurrentDiffTime;
    private static Thread sMainThread = Looper.getMainLooper().getThread();
    private static HandlerThread sTimerUpdateThread = MatrixHandlerThread.getNewHandlerThread("matrix_time_update_thread");
    private static Handler sTimeUpdateHandler = new Handler(sTimerUpdateThread.getLooper());
    private static final int METHOD_ID_MAX = 0xFFFFF;
    public static final int METHOD_ID_DISPATCH = METHOD_ID_MAX - 1;

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
            long currentTime = System.nanoTime() / Constants.TIME_MILLIS_TO_NANO;
            sCurrentDiffTime = currentTime - sLastDiffTime;
            if (isAlive) {
                sTimeUpdateHandler.postDelayed(this, Constants.TIME_UPDATE_CYCLE_MS);
            }
        }
    };

    public static MethodBeat getInstance() {
        return sInstance;
    }

    @Override
    public void onStart() {
        assert sBuffer != null;
        this.isAlive = true;
        updateDiffTime();
    }

    @Override
    public void onStop() {
        this.isAlive = false;
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

        if (Thread.currentThread() == sMainThread) {
            if (assertIn) {
                android.util.Log.e(TAG, "ERROR!!! MethodBeat.i Recursive calls!!!");
                return;
            }
            assertIn = true;
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, true);
            } else {
                sIndex = -1;
            }
            sLastIndex = sIndex;
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
        if (methodId >= METHOD_ID_MAX) {
            return;
        }
        if (Thread.currentThread() == sMainThread) {
            if (sIndex < Constants.BUFFER_SIZE) {
                mergeData(methodId, sIndex, false);
            } else {
                sIndex = -1;
            }
            sLastIndex = sIndex;
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
    }

    private static IndexRecord sIndexRecordHead = null;

    public IndexRecord maskIndex() {
        if (sIndexRecordHead == null) {
            sIndexRecordHead = new IndexRecord(sIndex - 1);
            return sIndexRecordHead;
        } else {
            IndexRecord indexRecord = new IndexRecord(sIndex - 1);
            IndexRecord record = sIndexRecordHead;
            IndexRecord last = null;
            for (; record != null; last = record, record = record.next) {
                if (indexRecord.index < record.index) {
                    IndexRecord tmp = record.next;
                    record.next = indexRecord;
                    indexRecord.next = tmp;
                    return indexRecord;
                }
            }
            last.next = indexRecord;
            return indexRecord;
        }
    }

    public void clearIndex(int index) {
        IndexRecord record = sIndexRecordHead;
        IndexRecord last = null;
        while (null != record) {
            if (record.index == index) {
                if (null != last) {
                    last.next = record.next;
                } else {
                    sIndexRecordHead = record.next;
                }
                record.next = null;
            }
            last = record;
            record = record.next;
        }
    }

    private static void checkPileup(int index) {
        IndexRecord indexRecord = sIndexRecordHead;
        while (indexRecord != null) {
            if (indexRecord.index == index || (indexRecord.index == -1 && sLastIndex == Constants.BUFFER_SIZE - 1)) {
                indexRecord.isValid = false;
                sIndexRecordHead = indexRecord = indexRecord.next;
            } else {
                break;
            }
        }
    }

    public final class IndexRecord {
        public IndexRecord(int index) {
            this.index = index;
        }

        public int index;
        private IndexRecord next;
        public boolean isValid = true;

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
    }

    public long[] copyData(IndexRecord start) {
        return copyData(start, new IndexRecord(sIndex - 1));
    }

    public long[] copyData(IndexRecord start, IndexRecord end) {

        long current = System.currentTimeMillis();
        try {
            if (start.isValid && end.isValid) {
                int length;
                long[] data = new long[0];
                if (end.index > start.index) {
                    length = end.index - start.index + 1;
                    data = new long[length];
                    System.arraycopy(sBuffer, start.index, data, 0, length);
                } else if (end.index < start.index) {
                    length = 1 + start.index + sBuffer.length - end.index;
                    data = new long[length];
                    System.arraycopy(sBuffer, start.index, data, 0, start.index + 1);
                    System.arraycopy(sBuffer, 0, data, start.index + 1, end.index + 1);
                }
                return data;
            }
            return new long[0];
        } finally {
            MatrixLog.i(TAG, "[copyData] cost:%sms", System.currentTimeMillis() - current);
        }
    }


}
