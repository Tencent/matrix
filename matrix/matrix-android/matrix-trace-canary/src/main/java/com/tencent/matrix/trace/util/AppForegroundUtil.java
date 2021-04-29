package com.tencent.matrix.trace.util;

import android.app.Activity;
import android.app.Application;
import android.app.Service;
import android.content.ComponentCallbacks2;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
import android.util.ArrayMap;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;


public enum AppForegroundUtil {

    INSTANCE;

    private static final String TAG = "Matrix.AppActiveDelegate";
    private boolean isAppForeground = false;
    private String visibleScene = "default";
    private Controller controller = new Controller();
    private boolean isInit = false;
    private String currentFragmentName;
    private Handler handler;

    public void init() {
        if (isInit) {
            return;
        }
        this.isInit = true;
        //application.registerComponentCallbacks(controller);
        //application.registerActivityLifecycleCallbacks(controller);
    }

    public String getCurrentFragmentName() {
        return currentFragmentName;
    }


    public void setCurrentFragmentName(String fragmentName) {
        this.currentFragmentName = fragmentName;
        updateScene(fragmentName);
    }

    public String getVisibleScene() {
        return visibleScene;
    }

    private void onDispatchForeground(String visibleScene) {
        isAppForeground = true;
        if (isAppForeground || !isInit) {
            return;
        }


    }

    private void onDispatchBackground(String visibleScene) {
        isAppForeground = false;
        if (!isAppForeground || !isInit) {
            return;
        }
    }

    public boolean isAppForeground() {
        return isAppForeground;
    }


    private final class Controller implements Application.ActivityLifecycleCallbacks, ComponentCallbacks2 {

        @Override
        public void onActivityStarted(Activity activity) {
            updateScene(activity);
            onDispatchForeground(getVisibleScene());
        }


        @Override
        public void onActivityStopped(Activity activity) {
            if (getTopActivityName() == null) {
                onDispatchBackground(getVisibleScene());
            }
        }


        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityDestroyed(Activity activity) {

        }

        @Override
        public void onActivityResumed(Activity activity) {

        }

        @Override
        public void onActivityPaused(Activity activity) {

        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

        }

        @Override
        public void onConfigurationChanged(Configuration newConfig) {

        }

        @Override
        public void onLowMemory() {

        }

        @Override
        public void onTrimMemory(int level) {
            if (level == TRIM_MEMORY_UI_HIDDEN && isAppForeground) { // fallback
                onDispatchBackground(visibleScene);
            }
        }
    }

    private void updateScene(Activity activity) {
        visibleScene = activity.getClass().getName();
    }

    private void updateScene(String currentFragmentName) {
        StringBuilder ss = new StringBuilder();
        ss.append(TextUtils.isEmpty(currentFragmentName) ? "?" : currentFragmentName);
        visibleScene = ss.toString();
    }

    /**
     * <p>Suppose {@code x} is a queue known to contain only strings.</p>
     * The following code can be used to dump the queue into a newly
     * allocated array of {@code String}:
     *
     * Note that {@code toArray(new Object[0])} is identical in function to
     * {@code toArray()}
     *
     * @return current TopActivityName
     * @throws NullPointerException if the specified array is null
     */
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
        }
        return null;
    }

    public static boolean isInterestingToUser() {
        return isActivityInterestingToUser();
    }

    private static boolean isServiceInterestingToUser() {
        try {
            Class activityThreadClass = Class.forName("android.app.ActivityThread");
            Object activityThread = activityThreadClass.getMethod("currentActivityThread").invoke(null);
            Field servicesField = activityThreadClass.getDeclaredField("mServices");
            servicesField.setAccessible(true);

            Map<Object, Object> services;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                services = (HashMap<Object, Object>) servicesField.get(activityThread);
            } else {
                services = (ArrayMap<Object, Object>) servicesField.get(activityThread);
            }
            if (services.size() < 1) {
                return false;
            }
            for (Object serviceObj : services.values()) {
                Class serviceClass = serviceObj.getClass();
                Service service = (Service) serviceObj;
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }


    private static boolean isActivityInterestingToUser() {
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
                return false;
            }
            for (Object activityRecord : activities.values()) {
                Class activityRecordClass = activityRecord.getClass();
                Field pausedField = activityRecordClass.getDeclaredField("paused");

                pausedField.setAccessible(true);
                if (!pausedField.getBoolean(activityRecord)) {
                    return true;
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
        return false;
    }
}
