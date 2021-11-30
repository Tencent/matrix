package com.tencent.matrix.openglleak.statistics;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.openglleak.statistics.source.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.source.ResRecordManager;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;
import com.tencent.matrix.util.MatrixLog;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

public class LeakMonitorForActivityLifecycle implements Application.ActivityLifecycleCallbacks {

    private static final String TAG = "matrix.LeakMonitorForActivityLifecycle";

    private Handler mH;
    private List<ActivityLeakMonitor> mActivityLeakMonitor;
    private List<MaybeLeakOpenGLInfo> mMaybeLeakList;
    private LeakListener mLeakListener;

    private static final long DOUBLE_CHECK_TIME = 0L;
    private static final long DOUBLE_CHECK_LOOPER = 1000L * 60 * 1;

    public LeakMonitorForActivityLifecycle() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
        mActivityLeakMonitor = new LinkedList<>();
        mMaybeLeakList = new LinkedList<>();

        mH.postDelayed(mDoubleCheck, DOUBLE_CHECK_LOOPER);
    }

    public void start(Application context) {
        MatrixLog.i(TAG, "start");
        context.registerActivityLifecycleCallbacks(this);
    }

    public void stop(Application context) {
        context.unregisterActivityLifecycleCallbacks(this);
    }

    @Override
    public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle bundle) {
        ActivityLeakMonitor activityLeakMonitor = new ActivityLeakMonitor(activity.hashCode(), new CustomizeLeakMonitor());
        activityLeakMonitor.start();

        synchronized (mActivityLeakMonitor) {
            mActivityLeakMonitor.add(activityLeakMonitor);
        }
    }

    @Override
    public void onActivityDestroyed(@NonNull Activity activity) {
        Integer activityHashCode = activity.hashCode();

        synchronized (mActivityLeakMonitor) {
            Iterator<ActivityLeakMonitor> it = mActivityLeakMonitor.iterator();
            while (it.hasNext()) {
                ActivityLeakMonitor item = it.next();
                if (null == item) {
                    continue;
                }

                if (item.getActivityHashCode() == activityHashCode) {
                    it.remove();

                    List<OpenGLInfo> leaks = item.end();
                    if (null != mLeakListener) {
                        for (OpenGLInfo leakItem : leaks) {
                            if (null != leakItem) {
                                mLeakListener.onMaybeLeak(leakItem);

                                synchronized (mMaybeLeakList) {
                                    // 可能泄漏，需要做 double check
                                    mMaybeLeakList.add(new MaybeLeakOpenGLInfo(leakItem));
                                }
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    public void setLeakListener(LeakMonitorForActivityLifecycle.LeakListener l) {
        mLeakListener = l;
    }

    private Runnable mDoubleCheck = new Runnable() {
        @Override
        public void run() {
            long now = System.currentTimeMillis();

            synchronized (mMaybeLeakList) {
                Iterator<MaybeLeakOpenGLInfo> it = mMaybeLeakList.iterator();
                while (it.hasNext()) {
                    MaybeLeakOpenGLInfo item = it.next();
                    if (null == item) {
                        continue;
                    }

                    if ((now - item.getMaybeLeakTime()) > DOUBLE_CHECK_TIME) {
                        it.remove();

                        if (null != mLeakListener) {
                            if (ResRecordManager.getInstance().isGLInfoRelease(item)) {
                                mLeakListener.onLeak(item);
                            }
                        }
                    }
                }
            }

            mH.postDelayed(mDoubleCheck, DOUBLE_CHECK_LOOPER);
        }
    };

    @Override
    public void onActivityStarted(@NonNull Activity activity) {

    }

    @Override
    public void onActivityResumed(@NonNull Activity activity) {

    }

    @Override
    public void onActivityPaused(@NonNull Activity activity) {

    }

    @Override
    public void onActivityStopped(@NonNull Activity activity) {

    }

    @Override
    public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle bundle) {

    }

    class ActivityLeakMonitor {
        private int mActivityHashCode;
        private CustomizeLeakMonitor mMonitor;

        ActivityLeakMonitor(int hashcode, CustomizeLeakMonitor m) {
            mActivityHashCode = hashcode;
            mMonitor = m;
        }

        void start() {
            if (null != mMonitor) {
                mMonitor.checkStart();
            }
        }

        List<OpenGLInfo> end() {
            List<OpenGLInfo> ret = new ArrayList<>();
            if (null == mMonitor) {
                return ret;
            }

            ret.addAll(mMonitor.checkEnd());

            return ret;
        }

        int getActivityHashCode() {
            return mActivityHashCode;
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

        long getMaybeLeakTime(long t) {
            return mMaybeLeakTime;
        }
    }

    public interface LeakListener {
        void onMaybeLeak(OpenGLInfo info);

        void onLeak(OpenGLInfo info);
    }

}
