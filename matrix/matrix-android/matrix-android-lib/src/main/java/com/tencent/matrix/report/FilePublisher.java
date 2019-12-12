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

package com.tencent.matrix.report;

import android.content.Context;
import android.content.SharedPreferences;

import com.tencent.matrix.util.MatrixUtil;

import java.util.HashMap;
import java.util.HashSet;

/**
 * Created by zhangshaowen on 2017/8/1.
 */

public class FilePublisher extends IssuePublisher {
    private static final String TAG = "Matrix.FilePublisher";

    private final long                     expiredTime;
    private final SharedPreferences        sharedPreferences;
    private final SharedPreferences.Editor editor;
    private final HashMap<String, Long>    mPublishedMap;
    protected final Context context;


    public FilePublisher(Context context, long expire, String tag, OnIssueDetectListener issueDetectListener) {
        super(issueDetectListener);
        this.context = context;
        expiredTime = expire;
        sharedPreferences = context.getSharedPreferences(tag + MatrixUtil.getProcessName(context), Context.MODE_PRIVATE);
        mPublishedMap = new HashMap<>();
        long current = System.currentTimeMillis();
        editor = sharedPreferences.edit();
        HashSet<String> spKeys = null;
        if (null != sharedPreferences.getAll()) {
            spKeys = new HashSet<>(sharedPreferences.getAll().keySet());
        }
        if (null != spKeys) {
            for (String key : spKeys) {
                long start = sharedPreferences.getLong(key, 0);
                long costTime = current - start;
                if (start <= 0 || costTime > expiredTime) {
                    editor.remove(key);
                } else {
                    mPublishedMap.put(key, start);
                }
            }
        }
        if (null != editor) {
            editor.apply();
        }
    }

    @Override
    public void markPublished(String key) {
        if (key == null) {
            return;
        }
        if (!mPublishedMap.containsKey(key)) {
            final long now = System.currentTimeMillis();
            mPublishedMap.put(key, now);
            SharedPreferences.Editor e = editor.putLong(key, now);
            if (null != e) {
                e.apply();
            }
            return;
        }
    }

    @Override
    public void unMarkPublished(String key) {
        if (key == null) {
            return;
        }
        if (!mPublishedMap.containsKey(key)) {
            return;
        }
        mPublishedMap.remove(key);
        SharedPreferences.Editor e = editor.remove(key);
        if (null != e) {
            e.apply();
        }
    }

    @Override
    public boolean isPublished(String key) {
        if (!mPublishedMap.containsKey(key)) {
            return false;
        }
        long start = mPublishedMap.get(key);
        if (start <= 0 || (System.currentTimeMillis() - start) > expiredTime) {
            SharedPreferences.Editor e = editor.remove(key);
            if (null != e) {
                e.apply();
            }
            mPublishedMap.remove(key);
            return false;
        }
        return true;
    }
}
