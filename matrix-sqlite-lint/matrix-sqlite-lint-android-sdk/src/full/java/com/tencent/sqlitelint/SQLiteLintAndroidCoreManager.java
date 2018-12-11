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

import android.content.Context;

import com.tencent.sqlitelint.behaviour.BaseBehaviour;
import com.tencent.sqlitelint.util.SLog;

import java.util.concurrent.ConcurrentHashMap;

/**
 * Manage the SQLiteLintAndroidCores
 * One concerned db one core
 *
 * @author liyongjie
 *         Created by liyongjie on 2017/6/14.
 */

public enum SQLiteLintAndroidCoreManager {
    INSTANCE;

    private static final String TAG = "SQLiteLint.SQLiteLintAndroidCoreManager";
    /**
     * key: the concerned db path
     * value: a SQLiteLintAndroidCore to check this db
     */
    private ConcurrentHashMap<String, SQLiteLintAndroidCore> mCoresMap = new ConcurrentHashMap<>();

    public void install(Context context, SQLiteLint.InstallEnv installEnv, SQLiteLint.Options options) {
        String concernedDbPath = installEnv.getConcernedDbPath();
        if (mCoresMap.containsKey(concernedDbPath)) {
            SLog.w(TAG, "install twice!! ignore");
            return;
        }

        SQLiteLintAndroidCore core = new SQLiteLintAndroidCore(context, installEnv, options);
        mCoresMap.put(concernedDbPath, core);
    }

    public void addBehavior(BaseBehaviour behaviour, String dbPath) {
        if (get(dbPath) == null) {
            return;
        }
        get(dbPath).addBehavior(behaviour);
    }

    public void removeBehavior(BaseBehaviour behaviour, String dbPath) {
        if (get(dbPath) == null) {
            return;
        }
        get(dbPath).removeBehavior(behaviour);
    }

    public SQLiteLintAndroidCore get(String dbPath) {
        return mCoresMap.get(dbPath);
    }

    public void remove(String dbPath) {
        mCoresMap.remove(dbPath);
    }
}
