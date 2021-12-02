package com.tencent.matrix.openglleak.statistics;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

public abstract class LeakMonitorDefault implements Application.ActivityLifecycleCallbacks {

    private static final String TAG = "matrix.LeakMonitorDefault";
    private List<ActivityLeakMonitor> mActivityLeakMonitor;

    protected LeakMonitorDefault() {
        mActivityLeakMonitor = new LinkedList<>();
    }

    public void start(Application context) {
        context.registerActivityLifecycleCallbacks(this);
        MatrixLog.i(TAG, "start");

        Activity currentActivity = getActivity();
        if (null != currentActivity) {
            ActivityLeakMonitor activityLeakMonitor = new ActivityLeakMonitor(currentActivity.hashCode(), new CustomizeLeakMonitor());
            activityLeakMonitor.start();

            synchronized (mActivityLeakMonitor) {
                mActivityLeakMonitor.add(activityLeakMonitor);
            }
        }
    }

    public void stop(Application context) {
        context.unregisterActivityLifecycleCallbacks(this);
        MatrixLog.i(TAG, "stop");
    }

    public abstract void onLeak(OpenGLInfo leak);

    @Override
    public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle bundle) {
        ActivityLeakMonitor activityLeakMonitor = new ActivityLeakMonitor(activity.hashCode(), new CustomizeLeakMonitor());
        activityLeakMonitor.start();

        MatrixLog.i(TAG, "onActivityCreated " + activityLeakMonitor);

        synchronized (mActivityLeakMonitor) {
            mActivityLeakMonitor.add(activityLeakMonitor);
        }
    }

    @Override
    public void onActivityDestroyed(@NonNull Activity activity) {
        Integer activityHashCode = activity.hashCode();
        MatrixLog.i(TAG, "onActivityDestroyed " + activityHashCode);

        synchronized (mActivityLeakMonitor) {
            List<OpenGLInfo> leaks = null;

            Iterator<ActivityLeakMonitor> it = mActivityLeakMonitor.iterator();
            while (it.hasNext()) {
                ActivityLeakMonitor activityLeakMonitor = it.next();
                if (null == activityLeakMonitor) {
                    continue;
                }

                if (activityLeakMonitor.getActivityHashCode() == activityHashCode) {
                    it.remove();

                    leaks = activityLeakMonitor.end();
                    for (OpenGLInfo leakItem : leaks) {
                        if (null != leakItem) {
                            if (leakItem.getActivityInfo().activityHashcode == activityLeakMonitor.mActivityHashCode) {
                                onLeak(leakItem);
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    @Override
    public void onActivityStarted(@NonNull Activity activity) {
        MatrixLog.i(TAG, "onActivityStarted " + activity.hashCode());
    }

    @Override
    public void onActivityResumed(@NonNull Activity activity) {
        MatrixLog.i(TAG, "onActivityResumed " + activity.hashCode());
    }

    @Override
    public void onActivityPaused(@NonNull Activity activity) {
        MatrixLog.i(TAG, "onActivityPaused " + activity.hashCode());
    }

    @Override
    public void onActivityStopped(@NonNull Activity activity) {
        MatrixLog.i(TAG, "onActivityStopped " + activity.hashCode());
    }

    @Override
    public void onActivitySaveInstanceState(@NonNull Activity activity, @NonNull Bundle bundle) {

    }

    public static Activity getActivity() {
        Class activityThreadClass = null;
        try {
            activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
            Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
            activitiesField.setAccessible(true);
            Map activities = (Map) activitiesField.get(activityThread);
            for (Object activityRecord : activities.values()) {
                Class activityRecordClass = activityRecord.getClass();
                Field pausedField = activityRecordClass.getDeclaredField("paused");
                pausedField.setAccessible(true);
                if (!pausedField.getBoolean(activityRecord)) {
                    Field activityField = activityRecordClass.getDeclaredField("activity");
                    activityField.setAccessible(true);
                    Activity activity = (Activity) activityField.get(activityRecord);
                    return activity;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return null;
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

            return mMonitor.checkEnd();
        }

        int getActivityHashCode() {
            return mActivityHashCode;
        }

        @Override
        public String toString() {
            return "ActivityLeakMonitor{" +
                    "mActivityHashCode=" + mActivityHashCode +
                    ", mMonitor=" + mMonitor +
                    '}';
        }
    }

}
