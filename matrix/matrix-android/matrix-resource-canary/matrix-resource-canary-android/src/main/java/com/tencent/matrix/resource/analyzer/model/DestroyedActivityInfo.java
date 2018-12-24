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

package com.tencent.matrix.resource.analyzer.model;

import android.app.Activity;

import java.lang.ref.WeakReference;

/**
 * Created by tangyinsheng on 2017/6/5.
 *
 * If you want to rename this class or any fields of it, please update the new names to
 * field <code>ActivityLeakAnalyzer.DESTROYED_ACTIVITY_INFO_CLASSNAME</code>,
 * <code>ActivityLeakAnalyzer.ACTIVITY_REFERENCE_KEY_FIELDNAME</code>,
 * <code>ActivityLeakAnalyzer.ACTIVITY_REFERENCE_FIELDNAME</code> in analyzer project.
 *
 */

public class DestroyedActivityInfo {
    public final String mKey;
    public final String mActivityName;

    public final WeakReference<Activity> mActivityRef;
    public final long mLastCreatedActivityCount;
    public int mDetectedCount = 0;

    public DestroyedActivityInfo(String key, Activity activity, String activityName, long lastCreatedActivityCount) {
        mKey = key;
        mActivityName = activityName;
        mActivityRef = new WeakReference<>(activity);
        mLastCreatedActivityCount = lastCreatedActivityCount;
    }
}
