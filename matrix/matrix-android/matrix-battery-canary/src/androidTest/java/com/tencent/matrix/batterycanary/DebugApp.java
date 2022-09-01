package com.tencent.matrix.batterycanary;

import android.app.Application;
import android.content.Context;

import androidx.multidex.MultiDex;

/**
 * @author Kaede
 * @since 22/8/2022
 */
public class DebugApp extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        MultiDex.install(base);
    }
}
