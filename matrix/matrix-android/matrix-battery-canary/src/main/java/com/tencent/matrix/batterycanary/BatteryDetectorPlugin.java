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

package com.tencent.matrix.batterycanary;

import android.app.Application;

import com.tencent.matrix.batterycanary.detector.BatteryDetectorConfig;
import com.tencent.matrix.batterycanary.detector.BatteryDetectorCore;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.util.MatrixLog;

/**
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */
public class BatteryDetectorPlugin extends Plugin {
    private static final String TAG = "Matrix.battery.BatteryDetectorPlugin";

    private final BatteryDetectorConfig mBatteryDetectorConfig;
    private final BatteryDetectorCore mCore;
    private       boolean stoppedForForeground = false;

//    public BatteryDetectorPlugin() {
//        mBatteryConfig = BatteryConfig.DEFAULT;
//    }

    public BatteryDetectorPlugin(BatteryDetectorConfig batteryDetectorConfig) {
        mBatteryDetectorConfig = batteryDetectorConfig;
        mCore = new BatteryDetectorCore(this);
    }

    public BatteryDetectorCore core() {
        return mCore;
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
    }

    @Override
    public synchronized void start() {
        if (!isPluginStarted() && !stoppedForForeground) {
            super.start();
            mCore.start();
        }
    }

    @Override
    public synchronized void stop() {
        stoppedForForeground = false;
        if (isPluginStarted()) {
            super.stop();
            mCore.stop();
        }
    }

    public BatteryDetectorConfig getConfig() {
        return mBatteryDetectorConfig;
    }

    @Override
    public void destroy() {
        super.destroy();
    }

    @Override
    public String getTag() {
        return "BatteryDetectorPlugin";
    }

    @Override
    public synchronized void onForeground(boolean isForground) {
        MatrixLog.i(TAG, "onForeground:" + isForground);

        super.onForeground(isForground);
        mCore.onForeground(isForground);

//        if (isForground && isPluginStarted()) {
//            stoppedForForeground = true;
//            super.stop();
//            mCore.stop();
//            return;
//        }
//
//        if (!isForground && isPluginStopped() && stoppedForForeground) {
//            super.start();
//            mCore.start();
//        }

    }
}
