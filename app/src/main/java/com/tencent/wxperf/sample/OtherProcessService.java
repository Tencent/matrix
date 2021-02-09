package com.tencent.wxperf.sample;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.support.annotation.Nullable;
import android.util.Log;

import com.tencent.components.backtrace.WeChatBacktrace;

public class OtherProcessService extends Service {

    private final static String TAG = "Wxperf.OtherProcess";

    boolean mInit = false;
    @Nullable
    @Override
    public IBinder onBind(Intent intent) {

        Log.e(TAG, "Service started");
        
        if (mInit) {
            return null;
        }

        mInit = true;

        WeChatBacktrace.instance().configure(getApplicationContext()).commit();

        return null;
    }

    public void onDestroy() {
        Log.e(TAG, "Service onDestroy");
    }
}
