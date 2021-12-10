package com.tencent.matrix.openglleak.statistics;

import android.app.Application;
import android.os.Handler;

import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

public class LeakMonitorForDoubleCheck extends LeakMonitorDefault {

    private static final String TAG = "matrix.LeakMonitorForActivityLifecycle";

    private Handler mH;
    private List<MaybeLeakOpenGLInfo> mMaybeLeakList;
    private LeakMonitorForDoubleCheck.LeakListener mLeakListener;

    private static final long DOUBLE_CHECK_LOOPER = 1000L * 30;
    private final long mDoubleCheckTime;
    private long mLastDoubleCheckTime;

    private Runnable mDoubleCheck = new Runnable() {
        @Override
        public void run() {
            doubleCheck();
        }
    };

    public LeakMonitorForDoubleCheck(long doubleCheckTime) {
        super();

        mDoubleCheckTime = doubleCheckTime;

        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        mMaybeLeakList = new LinkedList<>();
    }

    public void start(Application context) {
        super.start(context);
    }

    public void stop(Application context) {
        mH.removeCallbacks(mDoubleCheck);
        super.stop(context);
    }

    @Override
    public void onLeak(OpenGLInfo leak) {
        long now = System.currentTimeMillis();
        MaybeLeakOpenGLInfo leakItem = new MaybeLeakOpenGLInfo(leak);
        leakItem.setMaybeLeakTime(now);

        if (null != mLeakListener) {
            mLeakListener.onMaybeLeak(leakItem);
        }

        synchronized (mMaybeLeakList) {
            // 可能泄漏，需要做 double check
            mMaybeLeakList.add(leakItem);

            if ((now - mLastDoubleCheckTime) > DOUBLE_CHECK_LOOPER) {
                mH.removeCallbacks(mDoubleCheck);
                mH.post(mDoubleCheck);
            }
        }
    }

    public void setLeakListener(LeakListener l) {
        mLeakListener = l;
    }

    private void doubleCheck() {
        long now = System.currentTimeMillis();
        mLastDoubleCheckTime = now;

        synchronized (mMaybeLeakList) {
            Iterator<MaybeLeakOpenGLInfo> it = mMaybeLeakList.iterator();

            while (it.hasNext()) {
                MaybeLeakOpenGLInfo item = it.next();
                if (null == item) {
                    continue;
                }

                if ((now - item.getMaybeLeakTime()) > mDoubleCheckTime) {
                    it.remove();

                    if (null != mLeakListener) {
                        if (!ResRecordManager.getInstance().isGLInfoRelease(item)) {
                            mLeakListener.onLeak(item);
                        }
                    }
                }
            }

            if (mMaybeLeakList.size() > 0) {
                mH.removeCallbacks(mDoubleCheck);
                mH.postDelayed(mDoubleCheck, DOUBLE_CHECK_LOOPER);
            }
        }
    }

    class MaybeLeakOpenGLInfo extends OpenGLInfo {

        long mMaybeLeakTime;

        MaybeLeakOpenGLInfo(OpenGLInfo clone) {
            super(clone);
        }

        void setMaybeLeakTime(long t) {
            mMaybeLeakTime = t;
        }

        long getMaybeLeakTime() {
            return mMaybeLeakTime;
        }
    }

    public interface LeakListener {

        void onMaybeLeak(OpenGLInfo info);

        void onLeak(OpenGLInfo info);

    }
}
