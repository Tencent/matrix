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

package com.tencent.matrix.trace.constants;

/**
 * Created by caichongyang on 2017/5/26.
 */

public class Constants {

    public static final int BUFFER_SIZE = 100 * 10000; // 7.6M
    public static final int BUFFER_TMP_SIZE = 10000; // 70kB
    public static final int TIME_UPDATE_CYCLE_MS = 5;
    public static final long DEFAULT_EVIL_METHOD_THRESHOLD_MS = 1000L;
    public static final int DEFAULT_FPS_TIME_SLICE_ALIVE_MS = 6 * 1000;
    public static final float DEFAULT_DEVICE_REFRESH_RATE = 16.666667f;
    public static final int TIME_MILLIS_TO_NANO = 1000000;
    public static final int DEFAULT_ANR = 5 * 1000;
    public static final int DEFAULT_REPORT = 2 * 60 * 1000;
    public static final int DEFAULT_DROPPED_NORMAL = 2;
    public static final int DEFAULT_DROPPED_MIDDLE = 6;
    public static final int DEFAULT_DROPPED_HIGH = 12;
    public static final int DEFAULT_DROPPED_FROZEN = 42;
    public static final int DEFAULT_ENTER_ACTIVITY_THRESHOLD_MS = 3 * 1000;
    public static final int DEFAULT_STARTUP_THRESHOLD_MS_WARM = 6 * 1000;
    public static final int DEFAULT_STARTUP_THRESHOLD_MS = 8 * 1000;
    public static final int DEFAULT_RELEASE_BUFFER_DELAY = 30 * 1000;
    public static final int MAX_EVIL_METHOD_STACK = 20;
    public static final int MIN_EVIL_METHOD_RUN_COUNT = 5;
    public static final int MAX_EVIL_METHOD_DUR_TIME = DEFAULT_ANR + 1 * 1000;
    public static final int MAX_EVIL_METHOD_TRY_COUNT = 20;
    public static final float MAX_ANALYSE_METHOD_NEXT_PERCENT = .6f;
    public static final float MAX_ANALYSE_METHOD_ALL_PERCENT = .3f;
    public static final int MAX_LIMIT_ANALYSE_STACK_KEY_NUM = 10;
    public static final int LIMIT_WARM_THRESHOLD_MS = 2 * 1000;
    public static final int SUBTYPE_STARTUP_APPLICATION = 1;
    public static final int SUBTYPE_STARTUP_ACTIVITY = 2;
    public static final int MAX_EVIL_METHOD_COST = 6 * 1000;

    public static final boolean FPS_ENABLE = true;
    public static final boolean EVIL_METHOD_ENABLE = true;
    public static final String DEFAULT_SPLASH_ACTIVITY_NAME = "com.tencent.mm.app.WeChatSplashActivity";
    public static final String DEFAULT_CARE_SCENE_SET = "";
}
