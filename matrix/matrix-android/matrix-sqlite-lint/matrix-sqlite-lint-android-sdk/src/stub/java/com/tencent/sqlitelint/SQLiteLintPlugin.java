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

package com.tencent.sqlitelint;

import android.app.Application;

import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.plugin.PluginListener;
import com.tencent.sqlitelint.config.SQLiteLintConfig;

/**
 * //TEMP
 * @author liyongjie
 *         Created by liyongjie on 2017/6/29.
 */

public class SQLiteLintPlugin extends Plugin {

    public SQLiteLintPlugin(SQLiteLintConfig config) {
        if (config != null) {
        }
    }

    @Override
    public void init(Application app, PluginListener listener) {
        super.init(app, listener);
    }

    @Override
    public void start() {
        super.start();
    }

    @Override
    public void stop() {
        super.stop();
    }

    @Override
    public void destroy() {
        super.destroy();
    }

    @Override
    public String getTag() {
        return "SQLiteLint";
    }

    public void notifySqlExecution(String concernedDbPath, String sql, int timeCost) {
    }

    public void addConcernedDB(SQLiteLintConfig.ConcernDb concernDb) {
    }
}
