package com.tencent.matrix.openglleak.utils;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.tencent.matrix.openglleak.hook.OpenGLHook;
import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Field;
import java.util.Map;
import java.util.Objects;

public class ActivityRecorder implements Application.ActivityLifecycleCallbacks {

    private static final String TAG = "matrix.ActivityRecorder";

    private static final ActivityRecorder mInstance = new ActivityRecorder();

    private ActivityRecorder() {
    }

    public static ActivityRecorder getInstance() {
        return mInstance;
    }

    public void start(Application context) {
        Activity activity = getActivity();
        if (null != activity) {
            currentActivityInfo = new ActivityInfo(activity.hashCode(), activity.getLocalClassName());
            OpenGLHook.getInstance().updateCurrActivity(currentActivityInfo.toString());
        }
        context.registerActivityLifecycleCallbacks(this);
    }

    public void stop(Application context) {
        context.unregisterActivityLifecycleCallbacks(this);
    }

    private ActivityInfo currentActivityInfo = null;

    public ActivityInfo getCurrentActivityInfo() {
        return currentActivityInfo;
    }

    @Override
    public void onActivityCreated(@NonNull Activity activity, @Nullable Bundle bundle) {
        currentActivityInfo = new ActivityInfo(activity.hashCode(), activity.getLocalClassName());
        OpenGLHook.getInstance().updateCurrActivity(currentActivityInfo.toString());
    }

    @Override
    public void onActivityStarted(@NonNull Activity activity) {

    }

    @Override
    public void onActivityResumed(@NonNull Activity activity) {
        currentActivityInfo = new ActivityInfo(activity.hashCode(), activity.getLocalClassName());
        OpenGLHook.getInstance().updateCurrActivity(currentActivityInfo.toString());
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

    @Override
    public void onActivityDestroyed(@NonNull Activity activity) {
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


    public static ActivityInfo revertActivityInfo(String infoStr) {
        if (infoStr == null || infoStr.isEmpty()) {
            return new ActivityInfo(-1, "null");
        }

        try {
            String[] result = infoStr.split(" : ");
            int hash = Integer.parseInt(result[0]);
            String name = result[1];
            return new ActivityInfo(hash, name);
        } catch (Throwable t) {
            MatrixLog.printErrStackTrace(TAG, t, "");
        }
        return new ActivityInfo(-1, "");
    }

    public static class ActivityInfo {
        public int activityHashcode;
        public String name;

        ActivityInfo(int code, String n) {
            activityHashcode = code;
            name = n;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            ActivityInfo that = (ActivityInfo) o;
            return activityHashcode == that.activityHashcode &&
                    Objects.equals(name, that.name);
        }

        @Override
        public int hashCode() {
            return Objects.hash(activityHashcode, name);
        }

        @Override
        public String toString() {
            return activityHashcode + " : " + name;
        }
    }
}
