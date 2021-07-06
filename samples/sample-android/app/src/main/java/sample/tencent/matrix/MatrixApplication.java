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

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.iocanary.IOCanaryPlugin;
import com.tencent.matrix.iocanary.config.IOConfig;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.mrs.plugin.IDynamicConfig;
import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintPlugin;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

import sample.tencent.matrix.battery.BatteryCanaryInitHelper;
import sample.tencent.matrix.config.DynamicConfigImplDemo;
import sample.tencent.matrix.listener.TestPluginListener;
import sample.tencent.matrix.resource.ManualDumpActivity;
import sample.tencent.matrix.sqlitelint.TestSQLiteLintActivity;

/**
 * Created by caichongyang on 17/5/18.
 */

public class MatrixApplication extends Application {
    private static final String TAG = "Matrix.Application";

    private static Context sContext;

    // TODO Remove this.
    private static SQLiteLintConfig initSQLiteLintConfig() {
        try {
            /**
             * HOOK模式下，SQLiteLint会自己去获取所有已执行的sql语句及其耗时(by hooking sqlite3_profile)
             * @see 而另一个模式：SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY , 则需要调用 {@link SQLiteLint#notifySqlExecution(String, String, int)}来通知
             * SQLiteLint 需要分析的、已执行的sql语句及其耗时
             * @see TestSQLiteLintActivity#doTest()
             */
            return new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.HOOK);
        } catch (Throwable t) {
            return new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.HOOK);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();

        // Switch.
        DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo();

        sContext = this;
        MatrixLog.i(TAG, "Start Matrix configurations.");

        // Builder. Not necessary while some plugins can be configured separately.
        Matrix.Builder builder = new Matrix.Builder(this);

        // Reporter. Matrix will callback this listener when found issue then emitting it.
        builder.pluginListener(new TestPluginListener(this));

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

        Matrix.init(builder.build());

        MatrixLog.i(TAG, "Matrix configurations done.");

    }

    private TracePlugin configureTracePlugin(DynamicConfigImplDemo dynamicConfig) {

        boolean fpsEnable = dynamicConfig.isFPSEnable();
        boolean traceEnable = dynamicConfig.isTraceEnable();

        TraceConfig traceConfig = new TraceConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .enableFPS(fpsEnable)
                .enableEvilMethodTrace(traceEnable)
                .enableAnrTrace(traceEnable)
                .enableSignalAnrTrace(true)
                .enableStartup(traceEnable)
                .splashActivities("sample.tencent.matrix.SplashActivity;")
                .isDebug(true)
                .isDevEnv(false)
                .build();

        return new TracePlugin(traceConfig);
    }

    private ResourcePlugin configureResourcePlugin(DynamicConfigImplDemo dynamicConfig) {
        Intent intent = new Intent();
        ResourceConfig.DumpMode mode = ResourceConfig.DumpMode.MANUAL_DUMP;
        MatrixLog.i(TAG, "Dump Activity Leak Mode=%s", mode);
        intent.setClassName(this.getPackageName(), "com.tencent.mm.ui.matrix.ManualDumpActivity");
        ResourceConfig resourceConfig = new ResourceConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .setAutoDumpHprofMode(mode)
//                .setDetectDebuger(true) //matrix test code
//                    .set(intent)
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
        sqlLiteConfig = new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY);
        return new SQLiteLintPlugin(sqlLiteConfig);
    }

    private BatteryMonitorPlugin configureBatteryCanary() {
        // Configuration of battery plugin is really complicated.
        // See it in BatteryCanaryInitHelper.
        return BatteryCanaryInitHelper.createMonitor();
    }

    public static Context getContext() {
        return sContext;
    }
}
