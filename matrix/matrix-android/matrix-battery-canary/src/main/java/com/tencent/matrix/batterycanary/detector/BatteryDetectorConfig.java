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

package com.tencent.matrix.batterycanary.detector;

import com.tencent.mrs.plugin.IDynamicConfig;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */
public class BatteryDetectorConfig {
    private static final String TAG = "Matrix.BatteryConfig";

    private static final boolean  DETECT_WAKE_LOCK = true;
    // it's valid only when DETECT_WAKE_LOCK is valid
    private static final boolean RECORD_WAKE_LOCK = false;
    private static final boolean DETECT_ALARM     = true;
    private static final boolean RECORD_ALARM     = false;
//    private static final boolean DETECT_NONE      = false;

    /**
     * if a single wake lock is held longer than this threshold, a issue is published
     */
    private static final int DEFAULT_WAKE_LOCK_HOLD_TIME_THRESHOLD = 2 * 60 * 1000;

    /**
     * a wakelock of a same tag, may acquire and release many times.
     * There's a aggregation. Issues will be published if it acquires too often or keeps too long
     */
    private static final int DEFAULT_WAKE_LOCK_ACQUIRE_CNT_1H_THRESHOLD = 20;
    private static final int DEFAULT_WAKE_LOCK_HOLD_TIME_1H_THRESHOLD   = 10 * 60 * 1000;

    private static final int DEFAULT_ALARM_TRIGGERED_NUM_1H_THRESHOLD = 20;
    private static final int DEFAULT_WAKEUP_ALARM_TRIGGERED_NUM_1H_THRESHOLD = 20;

    /**
     * The default, lax policy will enable all available detectors
     */
//    public static final BatteryConfig DEFAULT = new BatteryConfig.Builder().build();

    private final IDynamicConfig mDynamicConfig;

    private BatteryDetectorConfig(IDynamicConfig dynamicConfig) {
        this.mDynamicConfig = dynamicConfig;
    }

    public boolean isDetectWakeLock() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_detect_wake_lock_enable.name(), DETECT_WAKE_LOCK);
    }
    public boolean isDetectAlarm() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_detect_alarm_enable.name(), DETECT_ALARM);
    }
    /**
     * only make sense when {@link #isDetectWakeLock()} is true
     *
     * @return
     */
    public boolean isRecordWakeLock() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_record_wake_lock_enable.name(), RECORD_WAKE_LOCK);
    }

    /**
     * only make sense when {@link #isDetectAlarm()} is true
     *
     * @return
     */
    public boolean isRecordAlarm() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_record_alarm_enable.name(), RECORD_ALARM);
    }

    public int getWakeLockHoldTimeThreshold() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_wake_lock_hold_time_threshold.name(), DEFAULT_WAKE_LOCK_HOLD_TIME_THRESHOLD);
    }

    public int getWakeLockHoldTime1HThreshold() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_wake_lock_1h_hold_time_threshold.name(), DEFAULT_WAKE_LOCK_HOLD_TIME_1H_THRESHOLD);
    }

    public int getWakeLockAcquireCnt1HThreshold() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_wake_lock_1h_acquire_cnt_threshold.name(), DEFAULT_WAKE_LOCK_ACQUIRE_CNT_1H_THRESHOLD);
    }

    public int getAlarmTriggerNum1HThreshold() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_alarm_1h_trigger_cnt_threshold.name(), DEFAULT_ALARM_TRIGGERED_NUM_1H_THRESHOLD);
    }

    public int getWakeUpAlarmTriggerNum1HThreshold() {
        return mDynamicConfig.get(IDynamicConfig.ExptEnum.clicfg_matrix_battery_wake_up_alarm_1h_trigger_cnt_threshold.name(), DEFAULT_WAKEUP_ALARM_TRIGGERED_NUM_1H_THRESHOLD);
    }


    @Override
    public String toString() {
        return String.format("[BatteryCanary.BatteryConfig], isDetectWakeLock:%b, isDetectAlarm:%b, isRecordWakeLock:%b, isRecordAlarm:%b",
                isDetectWakeLock(), isDetectWakeLock(), isRecordWakeLock(), isRecordAlarm());
    }

    public static final class Builder {
        private IDynamicConfig dynamicConfig;
        public Builder() {
        }
        public Builder dynamicConfig(IDynamicConfig dynamicConfig) {
            this.dynamicConfig = dynamicConfig;
            return this;
        }
        public BatteryDetectorConfig build() {
            return new BatteryDetectorConfig(dynamicConfig);
        }
    }
}
