package com.tencent.matrix.openglleak.statistics;

import android.app.Activity;
import android.app.Application;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.text.TextUtils;

import com.tencent.matrix.util.MatrixLog;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class LeakMonitor implements Application.ActivityLifecycleCallbacks {

    private static final String TAG = "matrix.GPU_LeakMonitor";
    private static LeakMonitor mInstance;

    private HandlerThread mHT;
    private Handler mH;

    private Map<WeakReference<Activity>, List<OpenGLInfo>> maps;
    private String currentActivityName = "";

    private long DOUBLE_CHECK_TIME = 1000 * 60 * 30;
    private final long DOUBLE_CHECK_LOOPER = 1000 * 60 * 1;

    private LeakMonitor() {
        mHT = new HandlerThread("LeakMonitor");
        mHT.start();
        mH = new Handler(mHT.getLooper());

        mH.postDelayed(doubleCheckRunnable, DOUBLE_CHECK_LOOPER);

        maps = new HashMap<>();
    }

    public static LeakMonitor getInstance() {
        if (mInstance == null) {
            mInstance = new LeakMonitor();
        }
        return mInstance;
    }

    public void setDoubleCheckTime(long doubleCheckTime) {
        DOUBLE_CHECK_TIME = doubleCheckTime;
    }

    public void setListener(LeakListener l) {
        OpenGLResRecorder.getInstance().setLeakListener(l);
    }

    public void start(Application context) {
        MatrixLog.i(TAG, "start");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            (context).registerActivityLifecycleCallbacks(mInstance);
        }
    }

    public String getCurrentActivityName() {
        return currentActivityName;
    }

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        currentActivityName = activity.getLocalClassName();

        MatrixLog.i(TAG, "activity oncreate:" + currentActivityName + "  :" + activity.hashCode());

        WeakReference<Activity> weakReference = new WeakReference<>(activity);
        List<OpenGLInfo> actvityList = OpenGLResRecorder.getInstance().getCopyList();

        maps.put(weakReference, actvityList);
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        MatrixLog.i(TAG, "activity ondestroy:" + activity.getLocalClassName() + "  :" + activity.hashCode());

        WeakReference<Activity> target = null;

        Set mapSet = maps.keySet();
        Iterator it = mapSet.iterator();
        while (it.hasNext()) {
            WeakReference<Activity> weakReference = (WeakReference) it.next();

            Activity a = (Activity) weakReference.get();
            if (a == null) {
                // 空的话移除
                it.remove();
                continue;
            }

            if ((a == activity)) {
                target = weakReference;
                break;
            }
        }

        // 找不到，跳过不处理
        if (target == null) {
            return;
        }

        String activityName = "";
        Activity aa = target.get();
        if (aa != null) {
            activityName = aa.getLocalClassName();
        }

        final String activityStr = activityName;
        final List<OpenGLInfo> createList = maps.get(target);
        final List<OpenGLInfo> destroyList = OpenGLResRecorder.getInstance().getCopyList();

        mH.post(new Runnable() {
            @Override
            public void run() {
                boolean result = findLeak(createList, destroyList);
                if (result) {
                    MatrixLog.i(TAG, activityStr + " leak! ");
                }
            }
        });

        // clear
        maps.remove(target);
    }

    private boolean findLeak(List<OpenGLInfo> createList, List<OpenGLInfo> destroyList) {
        if ((createList == null) || (destroyList == null)) {
            return false;
        }

        boolean hasLeak = false;

        for (OpenGLInfo destroy : destroyList) {
            if (destroy == null) {
                continue;
            }

            boolean isLeak;
            int i = 0;
            for (OpenGLInfo create : createList) {
                if (create == null) {
                    i = i + 1;
                    continue;
                }

                if ((create.getType() == destroy.getType())
                        && (create.getThreadId().equals(destroy.getThreadId())) && (create.getId()
                        == destroy.getId()) && (create.getEglContextNativeHandle() == destroy.getEglContextNativeHandle())) {
                    break;
                }

                i = i + 1;
            }

            if (i == createList.size()) {
                isLeak = true;
            } else {
                isLeak = false;
            }

            if (isLeak) {
                OpenGLResRecorder.getInstance().getNativeStack(destroy);
                OpenGLResRecorder.getInstance().setMaybeLeak(destroy);

                hasLeak = true;
            }
        }

        return hasLeak;
    }

    @Override
    public void onActivityStarted(Activity activity) {

    }

    @Override
    public void onActivityResumed(Activity activity) {
        currentActivityName = activity.getLocalClassName();
    }

    @Override
    public void onActivityPaused(Activity activity) {

    }

    @Override
    public void onActivityStopped(Activity activity) {

    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

    }

    public interface LeakListener {
        void onMaybeLeak(OpenGLInfo info);

        void onLeak(OpenGLInfo info);
    }

    private Runnable doubleCheckRunnable = new Runnable() {
        @Override
        public void run() {
            long now = System.currentTimeMillis();

            List<OpenGLInfo> copyList = OpenGLResRecorder.getInstance().getCopyList();
            MatrixLog.i(TAG, "double check list size:" + copyList.size());

            for (OpenGLInfo item : copyList) {
                if (item == null) {
                    continue;
                }

                if (item.getMaybeLeak()) {
                    if ((now - item.getMaybeLeakTime()) > DOUBLE_CHECK_TIME) {
                        OpenGLResRecorder.getInstance().setLeak(item);
                    }
                }
            }

            mH.postDelayed(doubleCheckRunnable, DOUBLE_CHECK_LOOPER);
        }
    };
}
