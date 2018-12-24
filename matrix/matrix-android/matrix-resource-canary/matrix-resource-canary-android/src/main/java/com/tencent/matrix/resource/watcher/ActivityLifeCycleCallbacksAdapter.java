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

package com.tencent.matrix.resource.watcher;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;

/**
 * Created by tangyinsheng on 2017/6/2.
 */

public class ActivityLifeCycleCallbacksAdapter implements Application.ActivityLifecycleCallbacks {

    @Override
    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
        // Override it if needed.
    }

    @Override
    public void onActivityStarted(Activity activity) {
        // Override it if needed.
    }

    @Override
    public void onActivityResumed(Activity activity) {
        // Override it if needed.
    }

    @Override
    public void onActivityPaused(Activity activity) {
        // Override it if needed.
    }

    @Override
    public void onActivityStopped(Activity activity) {
        // Override it if needed.
    }

    @Override
    public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
        // Override it if needed.
    }

    @Override
    public void onActivityDestroyed(Activity activity) {
        // Override it if needed.
    }
}
