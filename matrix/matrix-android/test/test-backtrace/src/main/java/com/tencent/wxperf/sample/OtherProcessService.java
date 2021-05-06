package com.tencent.wxperf.sample;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import com.tencent.components.backtrace.WeChatBacktrace;

public class OtherProcessService extends Service {

    private final static String TAG = "Backtrace.OtherProcess";

    boolean mInit = false;

    @Override
    public IBinder onBind(Intent intent) {

        Log.e(TAG, "Service started");

        if (mInit) {
            return null;
        }

        mInit = true;

        WeChatBacktrace.instance().configure(getApplicationContext())
                .directoryToWarmUp(getApplicationInfo().nativeLibraryDir)
                .directoryToWarmUp(WeChatBacktrace.getSystemLibraryPath())
                .isWarmUpProcess(false)
                .commit();

        return null;
    }
}
