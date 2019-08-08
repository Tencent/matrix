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

package com.tencent.matrix.batterycanary.core;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.WorkSource;

import com.tencent.matrix.batterycanary.BatteryCanaryPlugin;
import com.tencent.matrix.batterycanary.config.BatteryConfig;
import com.tencent.matrix.batterycanary.util.BatteryCanaryUtil;
import com.tencent.matrix.report.Issue;
import com.tencent.matrix.report.IssuePublisher;
//import com.tencent.matrix.util.MatrixLog;

/**
 * @author liyongjie
 *         Created by liyongjie on 2017/8/14.
 */

public class BatteryCanaryCore implements PowerManagerServiceHooker.IListener,
        AlarmManagerServiceHooker.IListener, IssuePublisher.OnIssueDetectListener {
    private static final String TAG = "Matrix.BatteryCanaryCore";

    private final BatteryConfig mBatteryConfig;
    private final BatteryCanaryDetectScheduler mDetectScheduler;
    private final BatteryCanaryPlugin mBatteryCanaryPlugin;

    private boolean           mIsStart;
    private WakeLockDetector mWakeLockDetector;
    private AlarmDetector mAlarmDetector;
    private final Context mContext;

    public BatteryCanaryCore(BatteryCanaryPlugin batteryCanaryPlugin) {
        mBatteryConfig = batteryCanaryPlugin.getConfig();
        mDetectScheduler = new BatteryCanaryDetectScheduler();
        mBatteryCanaryPlugin = batteryCanaryPlugin;
        mContext = batteryCanaryPlugin.getApplication();
    }

    public void start() {
        mDetectScheduler.start();
        initDetectorsAndHookers(mBatteryConfig);
        synchronized (this) {
            mIsStart = true;
        }
    }

    public synchronized boolean isStart() {
        return mIsStart;
    }

    public void stop() {
        synchronized (this) {
            mIsStart = false;
        }

        PowerManagerServiceHooker.removeListener(this);
        AlarmManagerServiceHooker.removeListener(this);

        mDetectScheduler.quit();
        mWakeLockDetector = null;
    }

    @Override
    public void onAcquireWakeLock(final IBinder token, final int flags, final String tag, final String packageName,
                                  final WorkSource workSource, final String historyTag) {

        if (mWakeLockDetector == null) {
            return;
        }

        final String stackTrace = BatteryCanaryUtil.getThrowableStack(new Throwable());
        final long acquireTime = System.currentTimeMillis();
        final Runnable detectTask = new Runnable() {
            @Override
            public void run() {
                mWakeLockDetector.onAcquireWakeLock(token, flags, tag, packageName,
                        workSource, historyTag, stackTrace, acquireTime);
            }
        };

        mDetectScheduler.addDetectTask(detectTask);
    }

    @Override
    public void onReleaseWakeLock(final IBinder token, final int flags) {
        if (mWakeLockDetector == null) {
            return;
        }

        final long releaseTime = System.currentTimeMillis();
        final Runnable detectTask = new Runnable() {
            @Override
            public void run() {
                mWakeLockDetector.onReleaseWakeLock(token, flags, releaseTime);
            }
        };

        mDetectScheduler.addDetectTask(detectTask);
    }

    @Override
    public void onDetectIssue(Issue issue) {
        mBatteryCanaryPlugin.onDetectIssue(issue);
    }

    private void initDetectorsAndHookers(BatteryConfig batteryConfig) {
        if (batteryConfig == null) {
            throw new RuntimeException("batteryConfig is null");
        }
        if (batteryConfig.isDetectWakeLock()) {
            mWakeLockDetector = new WakeLockDetector(this, batteryConfig, new WakeLockDetector.IDelegate() {
                @Override
                public void addDetectTask(Runnable detectTask, long delayInMillis) {
                    mDetectScheduler.addDetectTask(detectTask, delayInMillis);
                }

                @Override
                public boolean isScreenOn() {
                    PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
                    return pm.isScreenOn();
                }
            });
            PowerManagerServiceHooker.addListener(this);
        }


        if (batteryConfig.isDetectAlarm()) {
            mAlarmDetector = new AlarmDetector(this, mBatteryConfig);
            mDetectScheduler.addDetectTask(new Runnable() {
                @Override
                public void run() {
                    mAlarmDetector.initDetect();
                }
            });
            AlarmManagerServiceHooker.addListener(this);
        }
    }

    @Override
    public void onAlarmSet(final int type, final long triggerAtMillis, final long windowMillis, final long intervalMillis,
                           final int flags, final PendingIntent operation, final AlarmManager.OnAlarmListener onAlarmListener) {
//        MatrixLog.d(TAG, "onAlarmSet: type:%d, triggerAtMillis:%d, windowMillis:%d, intervalMillis:%d, flags:%d, operationInfo:%s, onAlarmListener:%s",
//                type, triggerAtMillis, windowMillis, intervalMillis, flags, operation, onAlarmListener);

        if (mAlarmDetector == null) {
            return;
        }

        final String stackTrace = BatteryCanaryUtil.getThrowableStack(new Throwable());
        final Runnable detectTask = new Runnable() {
            @Override
            public void run() {
                mAlarmDetector.onAlarmSet(type, triggerAtMillis, windowMillis, intervalMillis, flags, operation, onAlarmListener, stackTrace);
            }
        };

        mDetectScheduler.addDetectTask(detectTask);
    }

    @Override
    public void onAlarmRemove(final PendingIntent operation, final AlarmManager.OnAlarmListener onAlarmListener) {
//        MatrixLog.d(TAG, "onAlarmRemove: operationInfo:%s, onAlarmListener:%s", operation, onAlarmListener);

        final String stackTrace = BatteryCanaryUtil.getThrowableStack(new Throwable());
        final Runnable detectTask = new Runnable() {
            @Override
            public void run() {
                mAlarmDetector.onAlarmRemove(operation, onAlarmListener, stackTrace);
            }
        };

        mDetectScheduler.addDetectTask(detectTask);
    }
}
