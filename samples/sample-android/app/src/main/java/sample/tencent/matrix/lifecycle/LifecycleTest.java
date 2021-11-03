package sample.tencent.matrix.lifecycle;

import androidx.lifecycle.LifecycleOwner;

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
        MultiProcessLifecycleOwner.INSTANCE.addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "isForeground %s", isForeground);
            }
        });

        MultiProcessLifecycleOwner.INSTANCE.getResumedStateOwner().observeForever(new  IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: ON_RESUME");
            }
            @Override
            public void off() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: ON_PAUSE");
            }
        });

        MultiProcessLifecycleOwner.INSTANCE.getStartedStateOwner().observeForever(new  IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: ON_START");
            }
            @Override
            public void off() {
                MatrixLog.d(TAG, "MultiProcessLifecycleOwner: ON_STOP");
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
