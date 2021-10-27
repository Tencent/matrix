package sample.tencent.matrix.lifecycle;

import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import androidx.lifecycle.OnLifecycleEvent;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.CombinedProcessForegroundOwner;
import com.tencent.matrix.lifecycle.owners.MultiProcessLifecycleOwner;
import com.tencent.matrix.lifecycle.supervisor.ProcessSupervisor;
import com.tencent.matrix.listeners.IAppForeground;
import com.tencent.matrix.util.MatrixLog;

/**
 * Created by Yves on 2021/10/26
 */
public class LifecycleTest {
    private static final String TAG = "Matrix.sample.LifecycleTest";
    public static void test1() {
        MultiProcessLifecycleOwner.get().addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "isForeground %s", isForeground);
                if (!isForeground) {
                    // now change mode for transparent Activity
                    MultiProcessLifecycleOwner.get().setPauseAsBgIntervalMs(1000);
                }
            }
        });

        MultiProcessLifecycleOwner.get().getLifecycle().addObserver(new LifecycleObserver() {

            @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
            private void onProcessCreatedCalledOnce() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: onCreatedCalledOnce");
            }

            @OnLifecycleEvent(Lifecycle.Event.ON_START)
            private void onProcessStarted() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: onProcessStarted");
            }

            @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
            private void onProcessResumed() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: onProcessResumed");
            }

            @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
            private void onProcessPaused() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: onProcessPaused");
            }

            @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
            private void onProcessStopped() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: onProcessStopped");
            }
        });

        CombinedProcessForegroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "CombinedProcessForegroundStatefulOwner: ON");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "CombinedProcessForegroundStatefulOwner: OFF");
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
        CombinedProcessForegroundOwner.INSTANCE.observeWithLifecycle(owner, new IStateObserver() {
            @Override
            public void on() {

            }

            @Override
            public void off() {

            }
        });
    }
}
