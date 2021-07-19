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
    public static final int TIME_UPDATE_CYCLE_MS = 5;
    public static final int FILTER_STACK_MAX_COUNT = 60;
    public static final float FILTER_STACK_KEY_ALL_PERCENT = .3F;
    public static final float FILTER_STACK_KEY_PATENT_PERCENT = .8F;
    public static final int DEFAULT_EVIL_METHOD_THRESHOLD_MS = 700;
    public static final int DEFAULT_FPS_TIME_SLICE_ALIVE_MS = 10 * 1000;
    public static final int TIME_MILLIS_TO_NANO = 1000000;
    public static final int DEFAULT_INPUT_EXPIRED_TIME = 500;
    public static final int DEFAULT_ANR = 5 * 1000;
    public static final int DEFAULT_NORMAL_LAG = 2 * 1000;
    public static final int DEFAULT_IDLE_HANDLER_LAG = 2 * 1000;
    public static final int DEFAULT_ANR_INVALID = 6 * 1000;
    public static final long DEFAULT_FRAME_DURATION = 16666667L;

    public static final int DEFAULT_DROPPED_NORMAL = 3;
    public static final int DEFAULT_DROPPED_MIDDLE = 9;
    public static final int DEFAULT_DROPPED_HIGH = 24;
    public static final int DEFAULT_DROPPED_FROZEN = 42;

    public static final int DEFAULT_STARTUP_THRESHOLD_MS_WARM = 4 * 1000;
    public static final int DEFAULT_STARTUP_THRESHOLD_MS_COLD = 10 * 1000;

    public static final int DEFAULT_RELEASE_BUFFER_DELAY = 15 * 1000;
    public static final int TARGET_EVIL_METHOD_STACK = 30;
    public static final int MAX_LIMIT_ANALYSE_STACK_KEY_NUM = 10;

    public static final int LIMIT_WARM_THRESHOLD_MS = 5 * 1000;


    public enum Type {
        NORMAL, ANR, STARTUP, LAG, SIGNAL_ANR, LAG_IDLE_HANDLER, PRIORITY_MODIFIED, TIMERSLACK_MODIFIED
    }
}
