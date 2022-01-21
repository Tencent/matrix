package com.tencent.matrix.openglleak.utils;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.reflect.Field;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class ActivityRecorder implements Application.ActivityLifecycleCallbacks {

    private static final ActivityRecorder mInstance = new ActivityRecorder();
    private final List<ActivityInfo> mList = new LinkedList<>();

    private ActivityRecorder() {
    }

    public static ActivityRecorder getInstance() {
        return mInstance;
    }

    public void start(Application context) {
        Activity activity = getActivity();
        if (null != activity) {
            currentActivityInfo = new ActivityInfo(activity.hashCode(), activity.getLocalClassName());
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
        mList.add(currentActivityInfo);
    }

    @Override
    public void onActivityStarted(@NonNull Activity activity) {

    }

    @Override
    public void onActivityResumed(@NonNull Activity activity) {
        currentActivityInfo = new ActivityInfo(activity.hashCode(), activity.getLocalClassName());
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
        mList.remove(new ActivityInfo(activity.hashCode(), activity.getLocalClassName()));
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
            return "ActivityInfo{" +
                    "activityHashcode=" + activityHashcode +
                    ", name='" + name + '\'' +
                    '}';
        }
    }
}
