package com.tencent.matrix.openglleak.utils;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.LinkedList;
import java.util.List;
import java.util.Objects;

public class ActivityRecorder implements Application.ActivityLifecycleCallbacks {

    private static ActivityRecorder mInstance = new ActivityRecorder();
    private List<ActivityInfo> mList = new LinkedList<>();

    private ActivityRecorder() {
    }

    public static ActivityRecorder getInstance() {
        return mInstance;
    }

    public void start(Application context) {
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

    public class ActivityInfo {
        int hashcode;
        String name = "";

        ActivityInfo(int code, String n) {
            hashcode = code;
            name = n;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            ActivityInfo that = (ActivityInfo) o;
            return hashcode == that.hashcode &&
                    Objects.equals(name, that.name);
        }

        @Override
        public int hashCode() {
            return Objects.hash(hashcode, name);
        }

        @Override
        public String toString() {
            return "ActivityInfo{" +
                    "hashcode=" + hashcode +
                    ", name='" + name + '\'' +
                    '}';
        }
    }
}
