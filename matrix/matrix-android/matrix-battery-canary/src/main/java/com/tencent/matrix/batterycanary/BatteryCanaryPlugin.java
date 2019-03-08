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

import com.tencent.matrix.batterycanary.config.BatteryConfig;
import com.tencent.matrix.batterycanary.config.SharePluginInfo;
import com.tencent.matrix.batterycanary.core.BatteryCanaryCore;
import com.tencent.matrix.batterycanary.util.BatteryCanaryUtil;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

/**
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */

public class BatteryCanaryPlugin extends Plugin {
    private static final String TAG = "Matrix.BatteryCanaryPlugin";

    private final BatteryConfig mBatteryConfig;
    private       BatteryCanaryCore mCore;
    private       boolean stoppedForForeground = false;

//    public BatteryCanaryPlugin() {
//        mBatteryConfig = BatteryConfig.DEFAULT;
//    }

    public BatteryCanaryPlugin(BatteryConfig batteryConfig) {
        mBatteryConfig = batteryConfig;
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
        BatteryCanaryUtil.setPackageName(app);
        BatteryCanaryUtil.setProcessName(MatrixUtil.getProcessName(app));
        mCore = new BatteryCanaryCore(this);
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

    public BatteryConfig getConfig() {
        return mBatteryConfig;
    }

    @Override
    public void destroy() {
        super.destroy();
    }

    @Override
    public String getTag() {
        return SharePluginInfo.TAG_PLUGIN;
    }

    @Override
    public synchronized void onForeground(boolean isForground) {
        MatrixLog.i(TAG, "onForeground:" + isForground);

        super.onForeground(isForground);

        if (isForground && isPluginStarted()) {
            stoppedForForeground = true;
            super.stop();
            mCore.stop();
            return;
        }

        if (!isForground && isPluginStoped() && stoppedForForeground) {
            super.start();
            mCore.start();
        }


    }
}
