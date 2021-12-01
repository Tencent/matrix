/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package sample.tencent.matrix;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryEventDelegate;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.hook.pthread.PthreadHook;
import com.tencent.matrix.iocanary.IOCanaryPlugin;
import com.tencent.matrix.iocanary.config.IOConfig;
import com.tencent.matrix.lifecycle.owners.MatrixProcessLifecycleInitializer;
import com.tencent.matrix.lifecycle.supervisor.SupervisorConfig;
import com.tencent.matrix.memory.canary.MemoryCanaryPlugin;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.trace.tracer.SignalAnrTracer;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintPlugin;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

import java.io.File;
import java.util.ArrayList;

import sample.tencent.matrix.battery.BatteryCanaryInitHelper;
import sample.tencent.matrix.config.DynamicConfigImplDemo;
import sample.tencent.matrix.kt.memory.canary.MemoryCanaryBoot;
import sample.tencent.matrix.lifecycle.LifecycleTest;
import sample.tencent.matrix.listener.TestPluginListener;
import sample.tencent.matrix.resource.ManualDumpActivity;

/**
 * Created by caichongyang on 17/5/18.
 */

public class MatrixApplication extends Application {
    private static final String TAG = "Matrix.Application";

    private static Context sContext;

    public static boolean is64BitRuntime() {
        final String currRuntimeABI = Build.CPU_ABI;
        return "arm64-v8a".equalsIgnoreCase(currRuntimeABI)
                || "x86_64".equalsIgnoreCase(currRuntimeABI)
                || "mips64".equalsIgnoreCase(currRuntimeABI);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        if (!is64BitRuntime()) {
            try {
                final PthreadHook.ThreadStackShrinkConfig config = new PthreadHook.ThreadStackShrinkConfig()
                        .setEnabled(true)
                        .addIgnoreCreatorSoPatterns(".*/app_tbs/.*")
                        .addIgnoreCreatorSoPatterns(".*/libany\\.so$");
                HookManager.INSTANCE.addHook(PthreadHook.INSTANCE.setThreadStackShrinkConfig(config)).commitHooks();
            } catch (HookManager.HookFailedException e) {
                e.printStackTrace();
            }
        }

        try {
            ServiceInfo[] services = getPackageManager().getPackageInfo(getPackageName(), PackageManager.GET_SERVICES).services;
            for (ServiceInfo service : services) {
                MatrixLog.d(TAG, "name = %s, processName = %s", service.name, service.processName);
            }
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }

        // Switch.
        DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo();

        sContext = this;
        MatrixLog.i(TAG, "Start Matrix configurations.");

        // Builder. Not necessary while some plugins can be configured separately.
        Matrix.Builder builder = new Matrix.Builder(this);

        // Reporter. Matrix will callback this listener when found issue then emitting it.
        builder.pluginListener(new TestPluginListener(this));

        MemoryCanaryPlugin memoryCanaryPlugin = new MemoryCanaryPlugin(MemoryCanaryBoot.configure(this));
        builder.plugin(memoryCanaryPlugin);

        // Configure trace canary.
        TracePlugin tracePlugin = configureTracePlugin(dynamicConfig);
        builder.plugin(tracePlugin);

        // Configure resource canary.
        ResourcePlugin resourcePlugin = configureResourcePlugin(dynamicConfig);
        builder.plugin(resourcePlugin);

        // Configure io canary.
        IOCanaryPlugin ioCanaryPlugin = configureIOCanaryPlugin(dynamicConfig);
        builder.plugin(ioCanaryPlugin);

        // Configure SQLite lint plugin.
        SQLiteLintPlugin sqLiteLintPlugin = configureSQLiteLintPlugin();
        builder.plugin(sqLiteLintPlugin);

        // Configure battery canary.
        BatteryMonitorPlugin batteryMonitorPlugin = configureBatteryCanary();
        builder.plugin(batteryMonitorPlugin);

        builder.supervisorConfig(new SupervisorConfig(true, true, new ArrayList<String>()));
        builder.enableFgServiceMonitor(true);
        Matrix.init(builder.build());

        // Trace Plugin need call start() at the beginning.
//        tracePlugin.start();
//        memoryCanaryPlugin.start();

        Matrix.with().startAllPlugins();

        LifecycleTest.test1();

        MatrixLog.d("Yves-test", "hasCreatedActivities %s", MatrixProcessLifecycleInitializer.hasCreatedActivities());

        MatrixLog.i(TAG, "Matrix configurations done.");
    }

    private TracePlugin configureTracePlugin(DynamicConfigImplDemo dynamicConfig) {

        boolean fpsEnable = dynamicConfig.isFPSEnable();
        boolean traceEnable = dynamicConfig.isTraceEnable();
        boolean signalAnrTraceEnable = dynamicConfig.isSignalAnrTraceEnable();

        File traceFileDir = new File(getApplicationContext().getFilesDir(), "matrix_trace");
        if (!traceFileDir.exists()) {
            if (traceFileDir.mkdirs()) {
                MatrixLog.e(TAG, "failed to create traceFileDir");
            }
        }

        File anrTraceFile = new File(traceFileDir, "anr_trace");    // path : /data/user/0/sample.tencent.matrix/files/matrix_trace/anr_trace
        File printTraceFile = new File(traceFileDir, "print_trace");    // path : /data/user/0/sample.tencent.matrix/files/matrix_trace/print_trace

        TraceConfig traceConfig = new TraceConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .enableFPS(fpsEnable)
                .enableEvilMethodTrace(traceEnable)
                .enableAnrTrace(traceEnable)
                .enableStartup(traceEnable)
                .enableIdleHandlerTrace(traceEnable)                    // Introduced in Matrix 2.0
                .enableMainThreadPriorityTrace(true)                    // Introduced in Matrix 2.0
                .enableSignalAnrTrace(signalAnrTraceEnable)             // Introduced in Matrix 2.0
                .anrTracePath(anrTraceFile.getAbsolutePath())
                .printTracePath(printTraceFile.getAbsolutePath())
                .splashActivities("sample.tencent.matrix.SplashActivity;")
                .isDebug(true)
                .isDevEnv(false)
                .build();

        //Another way to use SignalAnrTracer separately
        //useSignalAnrTraceAlone(anrTraceFile.getAbsolutePath(), printTraceFile.getAbsolutePath());

        return new TracePlugin(traceConfig);
    }

    private void useSignalAnrTraceAlone(String anrFilePath, String printTraceFile) {
        SignalAnrTracer signalAnrTracer = new SignalAnrTracer(this, anrFilePath, printTraceFile);
        signalAnrTracer.setSignalAnrDetectedListener(new SignalAnrTracer.SignalAnrDetectedListener() {
            @Override
            public void onAnrDetected(String stackTrace, String mMessageString, long mMessageWhen, boolean fromProcessErrorState) {
                // got an ANR
            }
        });
        signalAnrTracer.onStartTrace();
    }

    private ResourcePlugin configureResourcePlugin(DynamicConfigImplDemo dynamicConfig) {
        Intent intent = new Intent();
        ResourceConfig.DumpMode mode = ResourceConfig.DumpMode.MANUAL_DUMP;
        MatrixLog.i(TAG, "Dump Activity Leak Mode=%s", mode);
        intent.setClassName(this.getPackageName(), "com.tencent.mm.ui.matrix.ManualDumpActivity");
        ResourceConfig resourceConfig = new ResourceConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .setAutoDumpHprofMode(mode)
                .setManualDumpTargetActivity(ManualDumpActivity.class.getName())
                .build();
        ResourcePlugin.activityLeakFixer(this);

        return new ResourcePlugin(resourceConfig);
    }

    private IOCanaryPlugin configureIOCanaryPlugin(DynamicConfigImplDemo dynamicConfig) {
        return new IOCanaryPlugin(new IOConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .build());
    }

    private SQLiteLintPlugin configureSQLiteLintPlugin() {
        SQLiteLintConfig sqlLiteConfig;

        /*
         * HOOK模式下，SQLiteLint会自己去获取所有已执行的sql语句及其耗时(by hooking sqlite3_profile)
         * @see 而另一个模式：SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY , 则需要调用 {@link SQLiteLint#notifySqlExecution(String, String, int)}来通知
         * SQLiteLint 需要分析的、已执行的sql语句及其耗时
         * @see TestSQLiteLintActivity#doTest()
         */
        // sqlLiteConfig = new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.HOOK);

        sqlLiteConfig = new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY);
        return new SQLiteLintPlugin(sqlLiteConfig);
    }

    private BatteryMonitorPlugin configureBatteryCanary() {
        // Configuration of battery plugin is really complicated.
        // See it in BatteryCanaryInitHelper.
        if (!BatteryEventDelegate.isInit()) {
            BatteryEventDelegate.init(this);
        }
        return BatteryCanaryInitHelper.createMonitor();
    }

    public static Context getContext() {
        return sContext;
    }
}
