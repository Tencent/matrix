package com.tencent.wxperf.sample;

import android.Manifest;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

import com.tencent.components.backtrace.WeChatBacktrace;
import com.tencent.components.backtrace.WeChatBacktraceTest;
import com.tencent.wxperf.jni.HookManager;
import com.tencent.wxperf.jni.memory.MemoryHook;
import com.tencent.wxperf.jni.test.UnwindBenckmarkTest;
import com.tencent.wxperf.jni.test.UnwindTest;

import java.io.File;

public class DemoActivity extends AppCompatActivity {

    private static final String TAG = "Wxperf.DemoActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_demo);

        Intent intent = new Intent();
        intent.setComponent(new ComponentName(this, OtherProcessService.class));
        bindService(intent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
            }
        }, BIND_AUTO_CREATE);

        initMemHook();

        checkPermission();
    }

    private void initMemHook() {
        // Init backtrace
        WeChatBacktrace.instance().configure(getApplicationContext())
                .directoryToWarmUp(getApplicationInfo().nativeLibraryDir)
                .directoryToWarmUp(WeChatBacktrace.getSystemLibraryPath())
                .setBacktraceMode(WeChatBacktrace.Mode.Quicken)
                .immediateGeneration(false)
                .isWarmUpProcess(false)
                .commit();

        try {
            HookManager.INSTANCE
                    // Memory hook
                    .addHook(MemoryHook.INSTANCE
                            .addHookSo(".*libwechatbacktrace_test\\.so$")
                            .enableStacktrace(true)
                            .stacktraceLogThreshold(0)
                            .enableMmapHook(false))
                    .commitHooks();
        } catch (HookManager.HookFailedException e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    private boolean checkPermission() {
        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_CONTACTS)
                != PackageManager.PERMISSION_GRANTED) {

            // Permission is not granted
            // Should we show an explanation?
            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                // Show an explanation to the user *asynchronously* -- don't block
                // this thread waiting for the user's response! After the user
                // sees the explanation, try again to request the permission.
            } else {
                // No explanation needed; request the permission
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        1);
                // MY_PERMISSIONS_REQUEST_READ_CONTACTS is an
                // app-defined int constant. The callback method gets the
                // result of the request.
            }
            return false;
        } else {
            // Permission has already been granted
            return true;
        }
    }

    private boolean mBacktraceTestInitialized = false;

    public void backtraceInit(View view) {

        if (mBacktraceTestInitialized) {
            return;
        }

        mBacktraceTestInitialized = true;

        // Init backtrace
        WeChatBacktraceTest.instance().configure(getApplicationContext())
                .directoryToWarmUp(getApplicationInfo().nativeLibraryDir)
                .directoryToWarmUp(WeChatBacktraceTest.getSystemLibraryPath())
                .setBacktraceMode(WeChatBacktraceTest.Mode.Quicken)
                .immediateGeneration(false)
                .isWarmUpProcess(true)
                .commit();
    }

    public void backtrace(View view) {
        UnwindBenckmarkTest.debugNative();
    }

    public void dump(View view) {
        java.io.File file = new File("/sdcard/dump/");
        file.mkdirs();
        MemoryHook.INSTANCE.dump(file.getAbsolutePath() + "/backtrace-test.log", file.getAbsolutePath() + "/backtrace-test.json");
    }

    public void testMapsUpdate(View view) {
        UnwindTest.init();
    }

    public void testWarmUp(View view) {
        WeChatBacktraceTest.instance().TestWarmUp();
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }
}
