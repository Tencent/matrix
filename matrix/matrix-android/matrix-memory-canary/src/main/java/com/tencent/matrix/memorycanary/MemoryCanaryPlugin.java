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

package com.tencent.matrix.memorycanary;

import android.app.Application;

import com.tencent.matrix.memorycanary.config.MemoryConfig;
import com.tencent.matrix.memorycanary.config.SharePluginInfo;
import com.tencent.matrix.memorycanary.core.MemoryCanaryCore;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.matrix.util.MatrixLog;

import org.json.JSONObject;

/**
 * @author zhouzhijie
 * Created by zhouzhijie on 2018/9/12.
 */

public class MemoryCanaryPlugin extends Plugin {
    private static final String TAG = "Matrix.MemoryCanaryPlugin";

    private final MemoryConfig mMemoryConfig;
    private MemoryCanaryCore mCore;

//    public MemoryCanaryPlugin() {
//        mMemoryConfig = MemoryConfig.DEFAULT;
//    }

    public MemoryCanaryPlugin(MemoryConfig config) {
        mMemoryConfig = config;
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
        mCore = new MemoryCanaryCore(this);
    }

    @Override
    public synchronized void start() {
        if (!isPluginStarted()) {
            super.start();
            mCore.start();
        }
    }

    @Override
    public synchronized void stop() {
        if (isPluginStarted()) {
            super.stop();
            mCore.stop();
        }
    }

    public MemoryConfig getConfig() {
        return mMemoryConfig;
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
    }

    @Override
    public JSONObject getJsonInfo() {
        return mCore.getJsonInfo();
    }
}
