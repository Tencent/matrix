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

package com.tencent.matrix.batterycanary.detector.config;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */

public class SharePluginInfo {
    public static final String TAG_PLUGIN = "battery";
    public static final String SUB_TAG_WAKE_LOCK = "wakeLock";
    public static final String SUB_TAG_ALARM = "alarm";

    public static final class IssueType {
        public static final int ISSUE_UNKNOWN                  = 0x0;
        public static final int ISSUE_WAKE_LOCK_ONCE_TOO_LONG  = 0x1;
        public static final int ISSUE_WAKE_LOCK_TOO_OFTEN      = 0x2;
        public static final int ISSUE_WAKE_LOCK_TOTAL_TOO_LONG = 0x3;

        public static final int ISSUE_ALARM_TOO_OFTEN          = 0x4;
        public static final int ISSUE_ALARM_WAKE_UP_TOO_OFTEN  = 0x5;
    }

    public static final String ISSUE_BATTERY_SUB_TAG                                   = "subTag";
    public static final String ISSUE_WAKE_LOCK_TAG                                     = "wakeLockTag";
    public static final String ISSUE_WAKE_LOCK_STACK_HISTORY                           = "stackHistory";
    public static final String ISSUE_WAKE_FLAGS                                        = "flags";
    public static final String ISSUE_WAKE_HOLD_TIME                                    = "holdTime";

    public static final String ISSUE_WAKE_LOCK_STATISTICAL_TIME_FRAME                  = "timeFrame";
    public static final String ISSUE_WAKE_LOCK_STATISTICAL_ACQUIRE_CNT                 = "acquireCnt";
    public static final String ISSUE_WAKE_LOCK_STATISTICAL_ACQUIRE_CNT_WHEN_SCREEN_OFF = "acquireCntWhenScreenOff";
    public static final String ISSUE_WAKE_LOCK_STATISTICAL_HOLD_TIME                   = "statisticalHoldTime";

    public static final String ISSUE_ALARM_TRIGGERED_NUM_1H                            = "alarmTriggeredNum1H";
    public static final String ISSUE_ALARMS_SET_STACKS                                 = "alarmSetStacks";
}
