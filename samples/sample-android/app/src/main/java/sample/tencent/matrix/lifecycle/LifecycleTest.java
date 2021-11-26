package sample.tencent.matrix.lifecycle;

import android.app.ActivityManager;
import android.content.Context;

import androidx.lifecycle.LifecycleOwner;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.DeepBackgroundOwner;
import com.tencent.matrix.lifecycle.owners.ExplicitBackgroundOwner;
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleOwner;
import com.tencent.matrix.lifecycle.owners.StagedBackgroundOwner;
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

/**
 * Created by Yves on 2021/10/26
 */
public class LifecycleTest {
    private static final String TAG = "Matrix.sample.LifecycleTest >>> " + MatrixUtil.getProcessName(Matrix.with().getApplication());
    public static void test1() {
        MatrixProcessLifecycleOwner.INSTANCE.addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "isForeground %s", isForeground);
            }
        });

        MatrixProcessLifecycleOwner.INSTANCE.getResumedStateOwner().observeForever(new  IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "MatrixProcessLifecycleOwner: ON_RESUME");
            }
            @Override
            public void off() {
                MatrixLog.d(TAG, "MatrixProcessLifecycleOwner: ON_PAUSE");
            }
        });

        MatrixProcessLifecycleOwner.INSTANCE.getStartedStateOwner().observeForever(new  IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "MatrixProcessLifecycleOwner: ON_START");
            }
            @Override
            public void off() {
                MatrixLog.d(TAG, "MatrixProcessLifecycleOwner: ON_STOP");
            }
        });

        ExplicitBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "ExplicitBackgroundOwner: ON");
//                ActivityManager am = (ActivityManager) Matrix.with().getApplication().getSystemService(Context.ACTIVITY_SERVICE);
//                for (ActivityManager.AppTask appTask : am.getAppTasks()) {
//                    appTask.finishAndRemoveTask();
//                }
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "ExplicitBackgroundOwner: OFF");
            }
        });

        StagedBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "StagedBackgroundOwner: ON");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "StagedBackgroundOwner: OFF");
            }
        });

        DeepBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "DeepBackgroundOwner: ON");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "DeepBackgroundOwner: OFF");
            }
        });


        ProcessSupervisor.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "ProcessSupervisor: on");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "ProcessSupervisor: off");
            }
        });

        AppActiveMatrixDelegate.INSTANCE.addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "AppActiveMatrixDelegate foreground %s", isForeground);
            }
        });
    }

    public static void test2(LifecycleOwner owner) {
        // auto remove when LifecycleOwner is destroyed
    }
}
