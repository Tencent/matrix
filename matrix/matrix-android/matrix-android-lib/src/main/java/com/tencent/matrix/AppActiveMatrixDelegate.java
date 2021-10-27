package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.os.Build;
import android.util.ArrayMap;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.lifecycle.owners.MultiProcessLifecycleOwner;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@Deprecated
public enum AppActiveMatrixDelegate {

    INSTANCE;

    public static final long DEFAULT_PAUSE_AS_BG_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(1);

    private static final String TAG = "Matrix.AppActiveDelegate";

    private final MultiProcessLifecycleOwner mLifecycleOwner = MultiProcessLifecycleOwner.get();

    public void init(Application application) {
    }

    public String getCurrentFragmentName() {
        return mLifecycleOwner.getCurrentFragmentName();
    }

    /**
     * must set after {@link Activity#onStart()}
     *
     * @param fragmentName
     */
    public void setCurrentFragmentName(String fragmentName) {
        mLifecycleOwner.setCurrentFragmentName(fragmentName);
    }

    public String getVisibleScene() {
        return mLifecycleOwner.getVisibleScene();
    }


    public boolean isAppForeground() {
        return mLifecycleOwner.isAppForeground();
    }

    public void addListener(IAppForeground listener) {
        mLifecycleOwner.addListener(listener);
    }

    public void removeListener(IAppForeground listener) {
        mLifecycleOwner.removeListener(listener);
    }

    public static String getTopActivityName() {
        long start = System.currentTimeMillis();
        try {
            Class activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
            Field activitiesField = activityThreadClass.getDeclaredField("mActivities");
            activitiesField.setAccessible(true);

            Map<Object, Object> activities;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                activities = (HashMap<Object, Object>) activitiesField.get(activityThread);
            } else {
                activities = (ArrayMap<Object, Object>) activitiesField.get(activityThread);
            }
            if (activities.size() < 1) {
                return null;
            }
            for (Object activityRecord : activities.values()) {
                Class activityRecordClass = activityRecord.getClass();
                Field pausedField = activityRecordClass.getDeclaredField("paused");
                pausedField.setAccessible(true);
                if (!pausedField.getBoolean(activityRecord)) {
                    Field activityField = activityRecordClass.getDeclaredField("activity");
                    activityField.setAccessible(true);
                    Activity activity = (Activity) activityField.get(activityRecord);
                    return activity.getClass().getName();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            long cost = System.currentTimeMillis() - start;
            MatrixLog.d(TAG, "[getTopActivityName] Cost:%s", cost);
        }
        return null;
    }
}
