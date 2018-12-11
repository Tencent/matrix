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

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanaryPlugin;
import com.tencent.matrix.batterycanary.config.BatteryConfig;
import com.tencent.matrix.iocanary.IOCanaryPlugin;
import com.tencent.matrix.memorycanary.MemoryCanaryPlugin;
import com.tencent.matrix.memorycanary.config.MemoryConfig;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.threadcanary.ThreadWatcher;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintPlugin;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

import java.util.HashSet;

import sample.tencent.matrix.sqlitelint.TestDBHelper;

/**
 * Created by caichongyang on 17/5/18.
 */

public class MatrixApplication extends Application {
    private static final String TAG = "Matrix.Application";

    private static Context sContext;

    @Override
    public void onCreate() {
        super.onCreate();
        sContext = this;
        MatrixLog.i(TAG, "MatrixApplication.onCreate");
        Matrix.Builder builder = new Matrix.Builder(this);
        TraceConfig TraceConfig = new TraceConfig.Builder()
                .fpsEnable(true)
                .methodTraceEnable(true)
                .fpsReportInterval(30 * 1000)
                .targetScene(null)
                .appStartUpThreshold(150)
                .splashName(SplashActivity.class.getName())
                .build();
        builder.plugin(new TracePlugin(TraceConfig));

        builder.plugin(new ThreadWatcher( 3000l, 6000l));

        ResourceConfig resourceConfig = new ResourceConfig.Builder()
                .setDetectIntervalMillis(20000)
                .setDumpHprof(true)
                .build();
        builder.plugin(new ResourcePlugin(resourceConfig));
        ResourcePlugin.activityLeakFixer(this);

        builder.plugin(new SQLiteLintPlugin(new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.HOOK)));
        BatteryConfig.Builder batteryConfigBuilder = new BatteryConfig.Builder()
                .enableDetectWakeLock()
                .enableDetectAlarm()
                .enableRecordAlarm()
                .wakeLockAcquireCnt1HThreshold(3)
                .wakeLockHoldTimeThreshold(20 * 1000)
                .wakeLockHoldTime1HThreshold(30 * 1000);
        builder.plugin(new BatteryCanaryPlugin(batteryConfigBuilder.build()));

        builder.plugin(new IOCanaryPlugin());

        HashSet<String> specials = new HashSet<>();
        specials.add("TestDeviceActivity");
        builder.plugin(new MemoryCanaryPlugin(new MemoryConfig.Builder().specialActivities(specials).build()));

        builder.patchListener(new TestPluginListener(this));
        Matrix.init(builder.build());

        Matrix.with().getPluginByClass(TracePlugin.class).start();
        Matrix.with().getPluginByClass(ResourcePlugin.class).start();
        Matrix.with().getPluginByClass(SQLiteLintPlugin.class).start();
        Matrix.with().getPluginByClass(IOCanaryPlugin.class).start();
        Matrix.with().getPluginByClass(BatteryCanaryPlugin.class).start();
        Matrix.with().getPluginByClass(MemoryCanaryPlugin.class).start();
        Matrix.with().getPluginByClass(ThreadWatcher.class).start();

        prepareSQLiteLint();
        MatrixLog.i("Matrix.HackCallback", "end:%s", System.currentTimeMillis());
    }


    private void prepareSQLiteLint() {
        SQLiteLintPlugin plugin = (SQLiteLintPlugin) Matrix.with().getPluginByClass(SQLiteLintPlugin.class);
        if (plugin == null) {
            return;
        }

        /*SQLiteLint.Options.Builder builder = new SQLiteLint.Options.Builder();
        builder.setAlertBehaviour(true).setReportBehaviour(true);
        SQLiteLint.InstallEnv installEnv = new SQLiteLint.InstallEnv(TestDBHelper.get().getWritableDatabase().getPath(),
                new SimpleSQLiteExecutionDelegate(TestDBHelper.get().getWritableDatabase()), SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY);*/
        plugin.addConcernedDB(new SQLiteLintConfig.ConcernDb(TestDBHelper.get().getWritableDatabase())
                //.setWhiteListXml(R.xml.sqlite_lint_whitelist)//disable white list by default
                .enableAllCheckers());
    }


    public static Context getContext() {
        return sContext;
    }
}
