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
import android.os.HandlerThread;
import android.os.Process;
import android.util.LongSparseArray;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitor;
import com.tencent.matrix.batterycanary.monitor.plugin.JiffiesMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.plugin.LooperTaskMonitorPlugin;
import com.tencent.matrix.iocanary.IOCanaryPlugin;
import com.tencent.matrix.iocanary.config.IOConfig;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.threadcanary.ThreadConfig;
import com.tencent.matrix.threadcanary.ThreadWatcher;
import com.tencent.matrix.trace.TracePlugin;
import com.tencent.matrix.trace.config.TraceConfig;
import com.tencent.matrix.util.MatrixHandlerThread;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.sqlitelint.SQLiteLint;
import com.tencent.sqlitelint.SQLiteLintPlugin;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

import java.util.List;

import sample.tencent.matrix.config.DynamicConfigImplDemo;
import sample.tencent.matrix.listener.TestPluginListener;

/**
 * Created by caichongyang on 17/5/18.
 */

public class MatrixApplication extends Application {
    private static final String TAG = "Matrix.Application";

    private static Context sContext;

    private static SQLiteLintConfig initSQLiteLintConfig() {
        try {
            return new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY);
        } catch (Throwable t) {
            return new SQLiteLintConfig(SQLiteLint.SqlExecutionCallbackMode.CUSTOM_NOTIFY);
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo();
        boolean matrixEnable = dynamicConfig.isMatrixEnable();
        boolean fpsEnable = dynamicConfig.isFPSEnable();
        boolean traceEnable = dynamicConfig.isTraceEnable();

        sContext = this;
        MatrixLog.i(TAG, "MatrixApplication.onCreate");

        Matrix.Builder builder = new Matrix.Builder(this);
        builder.patchListener(new TestPluginListener(this));

        //trace
        TraceConfig traceConfig = new TraceConfig.Builder()
                .dynamicConfig(dynamicConfig)
                .enableFPS(fpsEnable)
                .enableEvilMethodTrace(traceEnable)
                .enableAnrTrace(traceEnable)
                .enableStartup(traceEnable)
                .splashActivities("sample.tencent.matrix.SplashActivity;")
                .isDebug(true)
                .isDevEnv(false)
                .build();

        TracePlugin tracePlugin = (new TracePlugin(traceConfig));
        builder.plugin(tracePlugin);

        if (matrixEnable) {

            //resource
            builder.plugin(new ResourcePlugin(new ResourceConfig.Builder()
                    .dynamicConfig(dynamicConfig)
                    .setDumpHprof(false)
                    .setDetectDebuger(true)     //only set true when in sample, not in your app
                    .build()));
            ResourcePlugin.activityLeakFixer(this);

            //io
            IOCanaryPlugin ioCanaryPlugin = new IOCanaryPlugin(new IOConfig.Builder()
                    .dynamicConfig(dynamicConfig)
                    .build());
            builder.plugin(ioCanaryPlugin);


            // prevent api 19 UnsatisfiedLinkError
            //sqlite
            SQLiteLintConfig config = initSQLiteLintConfig();
            SQLiteLintPlugin sqLiteLintPlugin = new SQLiteLintPlugin(config);
            builder.plugin(sqLiteLintPlugin);

            ThreadWatcher threadWatcher = new ThreadWatcher(new ThreadConfig.Builder().dynamicConfig(dynamicConfig).build());
            builder.plugin(threadWatcher);

            BatteryMonitor batteryMonitor = new BatteryMonitor(new BatteryMonitor.Builder()
                    .installPlugin(LooperTaskMonitorPlugin.class)
                    .installPlugin(JiffiesMonitorPlugin.class)
                    .disableAppForegroundNotifyByMatrix(false)
                    .wakelockTimeout(2 * 60 * 1000)
                    .greyJiffiesTime(2 * 1000)
                    .build()
            );
            builder.plugin(batteryMonitor);


            MatrixHandlerThread.getDefaultHandler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    MatrixHandlerThread.getDefaultHandler().postDelayed(this, 200);
                }
            }, 2000);
        }

        Matrix.init(builder.build());

        //start only startup tracer, close other tracer.
        tracePlugin.start();
        Matrix.with().getPluginByClass(ThreadWatcher.class).start();
        Matrix.with().getPluginByClass(BatteryMonitor.class).start();
        MatrixLog.i("Matrix.HackCallback", "end:%s", System.currentTimeMillis());
    }


    public static Context getContext() {
        return sContext;
    }
}
