package com.tencent.matrix;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;


public enum AppForegroundDelegate {

    INSTANCE;

    private static final String TAG = "Matrix.AppForegroundDelegate";
    private static final String TAG_CARE = "Matrix.AppForeground";

    private Set<IAppForeground> listeners = Collections.synchronizedSet(new HashSet<IAppForeground>());
    private boolean isAppForeground = false;
    private String foregroundActivity = "default";
    private Controller controller = new Controller();
    private boolean isInited = false;

    public void init(Application application) {
        if (isInited) {
            MatrixLog.e(TAG, "has inited!");
            return;
        }
        this.isInited = true;

        application.registerActivityLifecycleCallbacks(controller);

    }

    public String getForegroundActivity() {
        return foregroundActivity;
    }

    private void onDispatchForeground(String activity) {
        if (isAppForeground || !isInited) {
            return;
        }

        MatrixLog.i(TAG_CARE, "onForeground... activity[%s]", activity);
        try {
            for (IAppForeground listener : listeners) {
                listener.onForeground(true);
            }
        } finally {
            isAppForeground = true;
        }
    }

    private void onDispatchBackground(String activity) {
        if (!isAppForeground || !isInited) {
            return;
        }
        MatrixLog.i(TAG_CARE, "onBackground... activity[%s]", activity);
        try {
            for (IAppForeground listener : listeners) {
                listener.onForeground(false);
            }
        } finally {
            isAppForeground = false;
        }
    }

    public boolean isAppForeground() {
        return isAppForeground;
    }

    public void addListener(IAppForeground listener) {
        listeners.add(listener);
    }

    public void removeListener(IAppForeground listener) {
        listeners.remove(listener);
    }


    private final class Controller implements Application.ActivityLifecycleCallbacks {

        private int activeCount = 0;
        private int ignoreCount = 0;
        private HashMap<String, Integer> checkMap = new HashMap<>();

        @Override
        public void onActivityStarted(Activity activity) {
            String name = activity.getClass().getName();
            MatrixLog.d(TAG, "onActivityStarted:%s", name);
            if (!isValid(name, true)) {
                return;
            }

            if (activeCount <= 0) {
                onDispatchForeground(name);
            }
            if (ignoreCount < 0) {
                MatrixLog.w(TAG, "[onActivityStarted] activity[%s] has changed Configurations!", name);
                ignoreCount++;
            } else {
                activeCount++;
            }
        }


        @Override
        public void onActivityStopped(Activity activity) {
            String name = activity.getClass().getName();
            MatrixLog.d(TAG, "onActivityStopped:%s", name);
            if (!isValid(name, false)) {
                return;
            }

            if (null != activity && activity.isChangingConfigurations()) {
                MatrixLog.w(TAG, "[onActivityStopped] activity[%s] is changing Configurations!", name);
                ignoreCount--;
            } else {
                activeCount--;
                if (activeCount <= 0) {
                    onDispatchBackground(name);
                }
            }
        }


        private boolean isValid(String name, boolean isStart) {
            if (isStart) {
                Integer integer = checkMap.get(name);
                if (integer == null) {
                    checkMap.put(name, 1);
                } else {
                    int count = integer;
                    checkMap.put(name, ++count);
                }
            } else {
                Integer integer = checkMap.get(name);
                if (integer == null || integer == 0) {
                    MatrixLog.w(TAG, "pass onActivityStopped:%s", name);
                    return false;
                } else {
                    int count = integer;
                    checkMap.put(name, --count);
                }
            }
            return true;
        }


        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityResumed(Activity activity) {
            foregroundActivity = activity.getClass().getName();
        }

        @Override
        public void onActivityPaused(Activity activity) {

        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

        }

        @Override
        public void onActivityDestroyed(Activity activity) {

        }


    }


}
