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

package sample.tencent.matrix.resource;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;
import android.view.View;

import com.tencent.matrix.Matrix;
import com.tencent.matrix.plugin.Plugin;
import com.tencent.matrix.resource.ResourcePlugin;
import com.tencent.matrix.resource.config.ResourceConfig;
import com.tencent.matrix.resource.config.SharePluginInfo;
import com.tencent.matrix.util.MatrixLog;
import com.tencent.matrix.util.MatrixUtil;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import sample.tencent.matrix.R;
import sample.tencent.matrix.issue.IssueFilter;

import static android.os.Build.VERSION.SDK_INT;

/**
 * Created by zhangshaowen on 17/6/13.
 */

public class TestLeakActivity extends Activity {
    private static final String TAG = "Matrix.TestLeakActivity";

    private static Set<Activity> testLeaks = new HashSet<>();

    private static ArrayList<Bitmap> bitmaps = new ArrayList<>();

    private NotificationManager mNotificationManager;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        SharedPreferences sharedPreferences = getSharedPreferences("memory" + MatrixUtil.getProcessName(TestLeakActivity.this), Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.clear();
        editor.commit();

        mNotificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        testLeaks.add(this);
        Plugin plugin = Matrix.with().getPluginByClass(ResourcePlugin.class);
        if (!plugin.isPluginStarted()) {
            MatrixLog.i(TAG, "plugin-resource start");
            plugin.start();
        }

//        BitmapFactory.Options options = new BitmapFactory.Options();
//        options.inSampleSize = 2;
//        bitmaps.add(BitmapFactory.decodeResource(getResources(), R.drawable.welcome_bg, options));
        MatrixLog.i(TAG, "test leak activity size: %d, bitmaps size: %d", testLeaks.size(), bitmaps.size());

        setContentView(R.layout.test_leak);

        IssueFilter.setCurrentFilter(IssueFilter.ISSUE_LEAK);
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onRestart() {
        super.onRestart();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        MatrixLog.i(TAG, "test leak activity destroy:" + this.hashCode());

//        new Thread(new Runnable() {
//            @Override
//            public void run() {
//                Runtime.getRuntime().gc();
//                Runtime.getRuntime().runFinalization();
//                Runtime.getRuntime().gc();
//            }
//        }).start();
    }

    private String getNotificationChannelIdCompat(Context context) {
        if (SDK_INT >= Build.VERSION_CODES.O) {
            String channelName = context.getString(com.tencent.matrix.resource.R.string.app_name);
            NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            NotificationChannel notificationChannel = notificationManager.getNotificationChannel(channelName);
            if (notificationChannel == null) {
                notificationChannel = new NotificationChannel(channelName, channelName, NotificationManager.IMPORTANCE_HIGH);
                notificationManager.createNotificationChannel(notificationChannel);
            }
            MatrixLog.v(TAG, "getNotificationChannelIdCompat: %s", channelName);
            return channelName;
        }
        MatrixLog.v(TAG, "getNotificationChannelIdCompat null");
        return null;
    }


    public void showNotification(View view) {
        Intent targetIntent = new Intent(this, ManualDumpActivity.class);
//        targetIntent.setClassName(getWatcher().getContext(), mTargetActivity);
//        targetIntent.putExtra(SharePluginInfo.ISSUE_ACTIVITY_NAME, destroyedActivityInfo.mActivityName);
//        targetIntent.putExtra(SharePluginInfo.ISSUE_REF_KEY, destroyedActivityInfo.mKey);
//        targetIntent.putExtra(SharePluginInfo.ISSUE_LEAK_PROCESS, MatrixUtil.getProcessName(context));


        PendingIntent pIntent = PendingIntent.getActivity(this, 0, targetIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        String dumpingHeapTitle = getString(com.tencent.matrix.resource.R.string.resource_canary_leak_tip);
        String dumpingHeapContent =
                String.format(Locale.getDefault(), "[%s] has leaked for [%s]min!!!",
                        "destroyedActivityInfo.mActivityName",
                        TimeUnit.MILLISECONDS.toMinutes(1));

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, getNotificationChannelIdCompat(this))
                .setContentTitle(dumpingHeapTitle)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setStyle(new NotificationCompat.BigTextStyle().bigText(dumpingHeapContent))
                .setContentIntent(pIntent)
                .setAutoCancel(true)
                .setSmallIcon(com.tencent.matrix.resource.R.drawable.ic_launcher)
                ;

        Notification notification = builder.build();

        mNotificationManager.notify(1000, notification);
    }

    private static final String CHANNEL_ID     = "com.tencent.mm.refdump.channel";
    private static final String DUMP_ACTION    = "com.tencent.mm.refdump.dump";

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {

            int importance = NotificationManager.IMPORTANCE_HIGH;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, CHANNEL_ID, importance);
            channel.setDescription(CHANNEL_ID);
            // Register the channel with the system; you can't change the importance
            // or other notification behaviors after this
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }

    public void showNotification2(View view) {
        createNotificationChannel();

        String msg = "msg";


        Intent dumpIntent = new Intent(DUMP_ACTION);
        PendingIntent pendingDumpIntent = PendingIntent.getBroadcast(this, 0, dumpIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        Notification notification = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_launcher)
                .setContentTitle("Your Activity LEAKED!!!")
                .setContentText(msg)
                .setPriority(NotificationCompat.PRIORITY_DEFAULT)
                .setStyle(new NotificationCompat.BigTextStyle().bigText(msg))
                .addAction(R.drawable.ic_launcher, "dump hprof", pendingDumpIntent)
//                .addAction(R.drawable.ic_launcher, "not remind", pendingDisableIntent)
                .build();

        NotificationManager notificationManager = getSystemService(NotificationManager.class);
        notificationManager.notify(1000, notification);
    }
}
