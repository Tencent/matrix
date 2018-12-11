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

package sample.tencent.matrix.battery;

import android.Manifest;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.widget.Toast;

import com.tencent.matrix.util.MatrixLog;

import sample.tencent.matrix.R;

/**
 * Created by zhangshaowen on 2017/8/15.
 */

public class TestBatteryActivity extends Activity {
    private static final String TAG = "Matrix.TestBatteryActivity";

    private static final int EXTERNAL_STORAGE_REQ_CODE = 0x1;
    private static final int WAKE_LOCK_REQ_CODE = 0x2;
    private PowerManager.WakeLock mWakeLock;
    private AlarmManager mAlarmManager;
    private PendingIntent mCurrentSetAlarmOperation;
    private AlarmManager.OnAlarmListener mCurrentSetOnAlarmListener;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_battery);

        requestPer();

        mAlarmManager = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case EXTERNAL_STORAGE_REQ_CODE:
                if (grantResults.length > 0
                    && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                }
                break;
        }
    }

    private void requestPer() {
        if (ContextCompat.checkSelfPermission(this,
            Manifest.permission.WRITE_EXTERNAL_STORAGE)
            != PackageManager.PERMISSION_GRANTED) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                Toast.makeText(this, "please give me the permission", Toast.LENGTH_SHORT).show();
            } else {
                ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    EXTERNAL_STORAGE_REQ_CODE);
            }
        }

        if (ContextCompat.checkSelfPermission(this,
            Manifest.permission.WAKE_LOCK)
            != PackageManager.PERMISSION_GRANTED) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                Manifest.permission.WAKE_LOCK)) {
                Toast.makeText(this, "please give me the permission", Toast.LENGTH_SHORT).show();
            } else {
                ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WAKE_LOCK},
                    WAKE_LOCK_REQ_CODE);
            }
        }
    }

    private void wakeLockAcquire() {
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "TestBattery-WakeLock");
            mWakeLock.setReferenceCounted(false);
        }

        mWakeLock.acquire();
    }

    private void wakeLockRelease() {
        if (mWakeLock == null) {
            return;
        }

        if (mWakeLock.isHeld())
            mWakeLock.release();
    }

    public void onClick(View v) {
        if (v.getId() == R.id.wake_lock_acquire) {
            wakeLockAcquire();
        } else if (v.getId() == R.id.wake_lock_release) {
            wakeLockRelease();
        } else if (v.getId() == R.id.set_alarm) {
            mCurrentSetAlarmOperation = setAlarmWithPICancelFlag();
            //mCurrentSetAlarmOperation = setAlarmWithPIUpdateFlag();
            //setMultiAlarms();
        } else if (v.getId() == R.id.cancel_alarm) {
            cancelAlarm(mCurrentSetAlarmOperation);
            //cancelMultiAlarms();
        }
    }


    private PendingIntent setAlarmWithPICancelFlag() {
        final long now = System.currentTimeMillis();
        PendingIntent pi = createTestPI(PendingIntent.FLAG_CANCEL_CURRENT, TestReceiver.TestBroadcastReceiver.class);
        mAlarmManager.setRepeating(AlarmManager.RTC, now + 300*1000, 60*1000, pi);
        return pi;
    }

    private PendingIntent setAlarmWithPIUpdateFlag() {
        final long now = System.currentTimeMillis();
        PendingIntent pi = createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver.class);
        mAlarmManager.setRepeating(AlarmManager.RTC, now + 300*1000, 60*1000, pi);
        MatrixLog.d(TAG, "setAlarmWithPIUpdateFlag pi hashCode:%d", pi.hashCode());
        return pi;
    }

    private void setMultiAlarms() {
        final long now = System.currentTimeMillis();
        final long nowElapsed = SystemClock.elapsedRealtime();
        mAlarmManager.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, nowElapsed + 7*60*1000, createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver.class));


        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mAlarmManager.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, now + 70*60*1000, createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver2.class));
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            mAlarmManager.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP, nowElapsed + 49*3600*1000, createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver3.class));
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            if (mCurrentSetOnAlarmListener == null) {
                mCurrentSetOnAlarmListener = new AlarmManager.OnAlarmListener() {
                    @Override
                    public void onAlarm() {
                        MatrixLog.d(TAG, "onAlarm time:%d", System.currentTimeMillis());
                    }
                };
            }
            mAlarmManager.setExact(AlarmManager.RTC, now + 5* 60 * 1000, "TestAlarm", mCurrentSetOnAlarmListener, null);
        }

        mAlarmManager.setInexactRepeating(AlarmManager.ELAPSED_REALTIME, nowElapsed+100000, 70, createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver4.class));
    }

    private void cancelMultiAlarms() {
        mAlarmManager.cancel(createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver.class));
        mAlarmManager.cancel(createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver2.class));
        mAlarmManager.cancel(createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver3.class));
        mAlarmManager.cancel(createTestPI(PendingIntent.FLAG_UPDATE_CURRENT, TestReceiver.TestBroadcastReceiver4.class));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && mCurrentSetOnAlarmListener != null) {
            mAlarmManager.cancel(mCurrentSetOnAlarmListener);
        }
    }

    private void cancelAlarm(PendingIntent operation) {
        if (mAlarmManager == null) {
            return;
        }

        mAlarmManager.cancel(operation);
    }

    private void cancelAlarm(AlarmManager.OnAlarmListener onAlarmListener) {
        if (mAlarmManager == null) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            if (onAlarmListener != null) {
                mAlarmManager.cancel(onAlarmListener);
            }
        }
    }

    private PendingIntent createTestPI(int flags, Class receiverCls) {
        Intent intent = new Intent();
        intent.setClass(this, receiverCls);

        PendingIntent pi = PendingIntent.getBroadcast(this, 0, intent, flags);
        return pi;
    }
}
