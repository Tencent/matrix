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

import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.batterycanary.BatteryCanary;
import com.tencent.matrix.batterycanary.BatteryMonitorPlugin;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback;
import com.tencent.matrix.batterycanary.monitor.BatteryMonitorCallback.BatteryPrinter.Printer;
import com.tencent.matrix.batterycanary.monitor.feature.CompositeMonitors;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.BatteryTmpSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.DeviceStatMonitorFeature.CpuFreqSnapshot;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature;
import com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot.ThreadJiffiesEntry;
import com.tencent.matrix.batterycanary.shell.ui.TopThreadIndicator;
import com.tencent.matrix.batterycanary.stats.BatteryStatsFeature;
import com.tencent.matrix.batterycanary.stats.ui.BatteryStatsActivity;
import com.tencent.matrix.batterycanary.utils.Consumer;
import com.tencent.matrix.util.MatrixLog;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import sample.tencent.matrix.R;
import sample.tencent.matrix.battery.stats.BatteryStatsSubProcActivity;

import static com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.JiffiesSnapshot;
import static com.tencent.matrix.batterycanary.monitor.feature.JiffiesMonitorFeature.Snapshot;

@SuppressLint("LongLogTag")
public class TestBatteryActivity extends Activity {
    private static final String TAG = "Matrix.TestBatteryActivity";
    private CompositeMonitors mCompositeMonitors;
    private Thread mBenchmarkThread;
    private int mBenchmarkTid = -1;

    // private PendingIntent getAlarmPendingIntent(final Context context, final int id, Intent intent) {
    //     PendingIntent pendingIntent;
    //     if (android.os.Build.VERSION.SDK_INT >= 29) {
    //         int requestCode = id % 450 + 50;
    //         pendingIntent = PendingIntent.getBroadcast(context, requestCode, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    //         MatrixLog.i(TAG, "getAlarmPendingIntent() id:%s requestCode:%s", id, requestCode);
    //     } else {
    //         pendingIntent = PendingIntent.getBroadcast(context, id, intent, PendingIntent.FLAG_CANCEL_CURRENT);
    //     }
    //     return pendingIntent;
    // }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.test_battery);

        BatteryCanaryInitHelper.startBatteryMonitor(this);

        BatteryMonitorPlugin plugin = Matrix.with().getPluginByClass(BatteryMonitorPlugin.class);
        mCompositeMonitors = new CompositeMonitors(plugin.core(), "manual_dump")
                .metricAll()
                .sample(CpuFreqSnapshot.class, 1000L)
                .sample(BatteryTmpSnapshot.class, 1000L);
        mCompositeMonitors.start();
        benchmark();

        //
        // final AlarmManager am = (AlarmManager) getApplicationContext().getSystemService(Context.ALARM_SERVICE);
        // if (am == null) {
        //     MatrixLog.e(TAG, "am == null");
        //     return;
        // }
        //
        // Intent intent = new Intent();
        // intent.setAction("ALARM_ACTION(" + String.valueOf(Process.myPid()) + ")");
        // intent.putExtra("ID", 1);
        // final PendingIntent pendingIntent = getAlarmPendingIntent(getApplicationContext(), (int) 1, intent);
        // if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
        //     am.setExact(AlarmManager.ELAPSED_REALTIME_WAKEUP, 200*1000, pendingIntent);
        // } else {
        //     am.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, 200*1000, pendingIntent);
        // }
        //
        // new Thread(new Runnable() {
        //     @Override
        //     public void run() {
        //         final AlarmManager am = (AlarmManager) getApplicationContext().getSystemService(Context.ALARM_SERVICE);
        //         am.cancel(pendingIntent);
        //         pendingIntent.cancel();
        //     }
        // }).start();


        // Test make notification
        // if (BatteryCanary.getMonitorFeature(NotificationMonitorFeature.class) != null) {
        //     tryNotify();
        // }

        showIndicator();
    }

    private void showIndicator() {
        if (!TopThreadIndicator.instance().isShowing()) {
            if (TopThreadIndicator.instance().checkPermission(this)) {
                TopThreadIndicator.instance().show(this);
                TopThreadIndicator.instance().start(5);
            }
        }
    }

    private void dismissIndicator() {
        TopThreadIndicator.instance().stop();
        TopThreadIndicator.instance().dismiss();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onRestart() {
        super.onRestart();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mCompositeMonitors.finish();

       // new Thread(new Runnable() {
       //     @Override
       //     public void run() {
       //         Runtime.getRuntime().gc();
       //         Runtime.getRuntime().runFinalization();
       //         Runtime.getRuntime().gc();
       //     }
       // }).start();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 2233) {
            showIndicator();
        }
    }

    private void benchmark() {
        mBenchmarkThread = new Thread(new Runnable() {
            @Override
            public void run() {
                mBenchmarkTid = Process.myTid();
                while (!isDestroyed()) {
                }
            }
        }, "Benchmark");
        mBenchmarkThread.start();
    }

    public void onShowIndicator(View view) {
        if (!TopThreadIndicator.instance().isShowing()) {
            if (TopThreadIndicator.instance().checkPermission(this)) {
                showIndicator();
            } else {
                TopThreadIndicator.instance().requestPermission(this, 2233);
            }
        }
    }

    public void onCloseIndicator(View view) {
        dismissIndicator();
    }

    public void onDumpBatteryStats(View view) {
        BatteryCanary.getMonitorFeature(BatteryStatsFeature.class, new Consumer<BatteryStatsFeature>() {
            @Override
            public void accept(BatteryStatsFeature batteryStatsFeature) {
                batteryStatsFeature.statsEvent("Manual Dump BatteryStats");
            }
        });
        tryDumpBatteryStats();
    }

    void tryDumpBatteryStats() {
        final CompositeMonitors compositeMonitors = mCompositeMonitors.fork();
        compositeMonitors.finish();

        // Figure out thread's callstack if need
        compositeMonitors.getDelta(JiffiesSnapshot.class, new Consumer<Snapshot.Delta<JiffiesSnapshot>>() {
            @Override
            public void accept(Snapshot.Delta<JiffiesSnapshot> jiffiesSnapshotDelta) {
                if (!jiffiesSnapshotDelta.dlt.threadEntries.getList().isEmpty()) {
                    for (JiffiesSnapshot.ThreadJiffiesEntry item : jiffiesSnapshotDelta.dlt.threadEntries.getList()) {
                        if (item.tid == mBenchmarkTid) {
                            item.stack = compositeMonitors.getMonitor().getConfig().callStackCollector.collect(mBenchmarkThread);
                        }
                    }
                }
            }
        });

        final Printer printer = new Printer();
        printer.writeTitle();
        new BatteryMonitorCallback.BatteryPrinter.Dumper().dump(compositeMonitors, printer);
        printer.writeEnding();

        BatteryStatsFeature statsFeat = BatteryCanary.getMonitorFeature(BatteryStatsFeature.class);
        if (statsFeat != null) {
            statsFeat.statsMonitors(compositeMonitors);
        }

        findViewById(R.id.tv_battery_stats).post(new Runnable() {
            @Override
            public void run() {
                String text = printer.toString();
                Log.i(TAG, "dump jiffies stats: " + text);
                TextView tv = findViewById(R.id.tv_battery_stats);
                tv.setVisibility(View.VISIBLE);
                tv.setText(text);

                compositeMonitors.getDelta(JiffiesMonitorFeature.JiffiesSnapshot.class, new Consumer<Snapshot.Delta<JiffiesSnapshot>>() {
                    @Override
                    public void accept(Snapshot.Delta<JiffiesSnapshot> jiffiesSnapshotDelta) {
                        if (jiffiesSnapshotDelta.dlt.threadEntries.getList().size() > 0) {
                            ThreadJiffiesEntry topThread = jiffiesSnapshotDelta.dlt.threadEntries.getList().get(0);
                            if (topThread.get() >= 1000) {
                                Toast.makeText(
                                        getApplication(),
                                        "Abnormal thread found: " + topThread.name + ", jiffies = " + topThread.get(),
                                        Toast.LENGTH_LONG
                                ).show();
                            }
                        }
                    }
                });
            }
        });
    }

    public void onCheckoutBatteryStats(View view) {
        if (BatteryCanary.getMonitorFeature(BatteryStatsFeature.class) == null) {
            Toast.makeText(this, "BatteryStatsFeature is not enabled, pls check 'BatteryCanaryInitHelper' to enable advanced features.", Toast.LENGTH_LONG).show();
            return;
        }
        Intent intent = new Intent(this, BatteryStatsActivity.class);
        this.startActivity(intent);
    }

    public void onCheckoutBatteryStatsSub(View view) {
        if (BatteryCanary.getMonitorFeature(BatteryStatsFeature.class) == null) {
            Toast.makeText(this, "BatteryStatsFeature is not enabled, pls check 'BatteryCanaryInitHelper' to enable advanced features.", Toast.LENGTH_LONG).show();
            return;
        }
        Intent intent = new Intent(this, BatteryStatsSubProcActivity.class);
        this.startActivity(intent);
    }

    void tryNotify() {
        NotificationManager notificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        String channelId = "TEST_CHANNEL_ID";
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel channel = new NotificationChannel(channelId, "TEST_CHANNEL_NAME", NotificationManager.IMPORTANCE_DEFAULT);
            manager.createNotificationChannel(channel);
        }

        Intent intent = new Intent(this, TestBatteryActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, 0);

        Notification notification = new NotificationCompat.Builder(this, channelId)
                .setContentTitle("NOTIFICATION_TILE")
                .setContentText(tryGetAppRunningNotificationText())
                .setContentIntent(pendingIntent)
                .setAutoCancel(true)
                .setSmallIcon(com.tencent.matrix.batterycanary.R.drawable.ic_launcher)
                .setWhen(System.currentTimeMillis())
                .build();

        notificationManager.notify(16657, notification);
    }

    private String tryGetAppRunningNotificationText() {
        Resources resources = Resources.getSystem();
        if (resources != null) {
            int appRunningNotifyTextId = resources.getIdentifier(
                    "app_running_notification_text",
                    "string",
                    "android"
            );
            if (appRunningNotifyTextId > 0) {
                return resources.getString(appRunningNotifyTextId);
            }
        }
        return null;
    }

    @SuppressLint("NewApi")
    public void onStartAnim(final View view) {
        ValueAnimator animator = ValueAnimator.ofArgb(0xff94E1F7, 0xffF35519);
        animator.setDuration(1000);
        animator.setRepeatCount(5);
        animator.setRepeatMode(ValueAnimator.REVERSE);
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                Button.class.cast(view).setTextColor((Integer) animation.getAnimatedValue());
            }
        });
        animator.start();

        view.post(new Runnable() {
            @Override
            public void run() {
                Button.class.cast(view).setText("Changing Text Color");
                dumpWindowViewTree();
            }
        });
    }

    private static void dumpWindowViewTree() {
        try {
            Class<?> wmgClass = Class.forName("android.view.WindowManagerGlobal");
            Object wmgInstance = wmgClass.getMethod("getInstance").invoke(null);
            String[] rootViews = (String[]) wmgClass.getDeclaredMethod("getViewRootNames").invoke(wmgInstance);
            if (rootViews != null) {
                for (String item : rootViews) {
                    Object rootView = wmgClass.getDeclaredMethod("getRootView", String.class).invoke(wmgInstance, item);
                    if (rootView != null ) {
                        String string = getViewHierarchy((View) rootView);
                        MatrixLog.i(TAG, "window = \t\n" + string);
                    }
                }
            }
        } catch (Throwable e) {
            MatrixLog.e(TAG,"getBallInfoListSync fail!", e);
        }
    }



    private static String getViewHierarchy(@NonNull View v) {
        StringBuilder desc = new StringBuilder();
        getViewHierarchy(v, desc, 0);
        return desc.toString();
    }

    private static void getViewHierarchy(View v, StringBuilder desc, int margin) {
        desc.append(getViewMessage(v, margin));
        if (v instanceof ViewGroup) {
            margin++;
            ViewGroup vg = (ViewGroup) v;
            for (int i = 0; i < vg.getChildCount(); i++) {
                getViewHierarchy(vg.getChildAt(i), desc, margin);
            }
        }
    }

    @SuppressLint("ResourceType")
    private static String getViewMessage(View v, int marginOffset) {
        String repeated = new String(new char[marginOffset]).replace("\0", "  ");
        try {
             String resourceId = v.getResources() != null ? (v.getId() > 0 ? v.getResources().getResourceName(v.getId()) : "no_id") : "no_resources";
            return repeated + "[" + v.getClass().getSimpleName() + "] " + resourceId + "\n";
        } catch (Resources.NotFoundException e) {
            return repeated + "[" + v.getClass().getSimpleName() + "] name_not_found\n";
        }
    }
}
