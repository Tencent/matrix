package sample.tencent.matrix.lifecycle;

import androidx.lifecycle.LifecycleOwner;

import com.tencent.matrix.AppActiveMatrixDelegate;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.lifecycle.IStateObserver;
import com.tencent.matrix.lifecycle.owners.OverlayWindowLifecycleOwner;
import com.tencent.matrix.lifecycle.owners.ProcessDeepBackgroundOwner;
import com.tencent.matrix.lifecycle.owners.ProcessExplicitBackgroundOwner;
import com.tencent.matrix.lifecycle.owners.ProcessStagedBackgroundOwner;
import com.tencent.matrix.lifecycle.owners.ProcessUILifecycleOwner;
import com.tencent.matrix.lifecycle.owners.ProcessUIResumedStateOwner;
import com.tencent.matrix.lifecycle.owners.ProcessUIStartedStateOwner;
import com.tencent.matrix.lifecycle.supervisor.AppExplicitBackgroundOwner;
import com.tencent.matrix.lifecycle.supervisor.AppStagedBackgroundOwner;
import com.tencent.matrix.lifecycle.supervisor.AppUIForegroundOwner;
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
        ProcessUILifecycleOwner.INSTANCE.addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "isForeground %s", isForeground);
            }
        });

        ProcessUIResumedStateOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "ProcessUILifecycleOwner: ON_RESUME");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "ProcessUILifecycleOwner: ON_PAUSE");
            }
        });

        ProcessUIStartedStateOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "ProcessUILifecycleOwner: ON_START");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "ProcessUILifecycleOwner: ON_STOP");
            }
        });

        ProcessExplicitBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
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

        ProcessStagedBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "StagedBackgroundOwner: ON");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "StagedBackgroundOwner: OFF");
            }
        });

        ProcessDeepBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "DeepBackgroundOwner: ON");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "DeepBackgroundOwner: OFF");
            }
        });

        AppUIForegroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "AppUI: on ...... %s", ProcessSupervisor.INSTANCE.getRecentScene());
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "AppUI: off ...... %s", ProcessSupervisor.INSTANCE.getRecentScene());
            }
        });

        AppStagedBackgroundOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "AppStagedBg: on");
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "AppStagedBg: off");
            }
        });

        AppExplicitBackgroundOwner.INSTANCE.observeForever(
                new IStateObserver() {
                    @Override
                    public void on() {
                        MatrixLog.d(TAG, "AppExplicitBg: on");
                    }

                    @Override
                    public void off() {
                        MatrixLog.d(TAG, "AppExplicitBg: off");
                    }
                }
        );

        AppActiveMatrixDelegate.INSTANCE.addListener(new IAppForeground() {
            @Override
            public void onForeground(boolean isForeground) {
                MatrixLog.d(TAG, "AppActiveMatrixDelegate foreground %s", isForeground);
            }
        });

        OverlayWindowLifecycleOwner.INSTANCE.observeForever(new IStateObserver() {
            @Override
            public void on() {
                MatrixLog.d(TAG, "OverlayWindowLifecycleOwner: on , hasOverlayWindow %s, hasVisibleWindow %s", OverlayWindowLifecycleOwner.INSTANCE.hasOverlayWindow(), OverlayWindowLifecycleOwner.INSTANCE.hasVisibleWindow());
            }

            @Override
            public void off() {
                MatrixLog.d(TAG, "OverlayWindowLifecycleOwner: off , hasOverlayWindow %s, hasVisibleWindow %s", OverlayWindowLifecycleOwner.INSTANCE.hasOverlayWindow(), OverlayWindowLifecycleOwner.INSTANCE.hasVisibleWindow());
            }
        });
    }

    public static void test2(LifecycleOwner owner) {
        // auto remove when LifecycleOwner is destroyed
    }
}
