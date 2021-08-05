package com.tencent.matrix.test.memoryhook;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.tencent.matrix.backtrace.WarmUpReporter;
import com.tencent.matrix.backtrace.WeChatBacktrace;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.hook.memory.MemoryHook;
import com.tencent.matrix.hook.pthread.PthreadHook;

public class MemoryHookActivity extends AppCompatActivity {

    private static final String TAG = "Matrix.TestHook";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_demo);

        checkPermission();

        backtraceInit();
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

    private void warmedUpToast() {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MemoryHookActivity.this, "Warm-up has been done!", Toast.LENGTH_LONG).show();
            }
        });

    }

    private boolean mBacktraceTestInitialized = false;

    private Handler mHandler = new Handler(Looper.getMainLooper());

    public static boolean is64BitRuntime() {
        final String currRuntimeABI = Build.CPU_ABI;
        return "arm64-v8a".equalsIgnoreCase(currRuntimeABI)
                || "x86_64".equalsIgnoreCase(currRuntimeABI)
                || "mips64".equalsIgnoreCase(currRuntimeABI);
    }

    public void backtraceInit() {

        if (mBacktraceTestInitialized) {
            return;
        }

        mBacktraceTestInitialized = true;


        // Reporter
        WeChatBacktrace.setReporter(new WarmUpReporter() {
            @Override
            public void onReport(ReportEvent type, Object... args) {
                if (type == ReportEvent.WarmedUp) {
                    Log.i(TAG, String.format("WeChat QUT has warmed up."));
                } else if (type == ReportEvent.WarmUpDuration && args.length == 1) {
                    Log.i(TAG, String.format("WeChat QUT Warm-up duration: %sms", (long) args[0]));
                }
            }
        });

        // Init backtrace
        if (is64BitRuntime()) {
            WeChatBacktrace.instance()
                    .configure(getApplication())
                    .setBacktraceMode(WeChatBacktrace.Mode.Fp)
                    .setQuickenAlwaysOn()
                    .commit();
        } else {
            WeChatBacktrace.instance()
                    .configure(getApplication())
                    .warmUpSettings(WeChatBacktrace.WarmUpTiming.PostStartup, 0)
                    .directoryToWarmUp(WeChatBacktrace.getSystemFrameworkOATPath() + "boot.oat")
                    .directoryToWarmUp(
                            WeChatBacktrace.getSystemFrameworkOATPath() + "boot-framework.oat")
                    .commit();
        }

        if (WeChatBacktrace.hasWarmedUp(this)) {
            warmedUpToast();
        }

    }

    boolean mHasPrepared = false;

    @SuppressLint("UnsafeDynamicallyLoadedCode")
    public void prepareData(View view) {
        if (mHasPrepared) return;
        mHasPrepared = true;

        System.loadLibrary("test-memoryhook");

        MemoryHookTestNative.nativePrepareData();
    }

    boolean mHasRunTest = false;

    public void runTest(View view) {

        if (!mHasPrepared || mHasRunTest) return;
        mHasRunTest = true;

        // Init Hooks.
        try {
            HookManager.INSTANCE

                    // Memory hook
                    .addHook(MemoryHook.INSTANCE
//                            .addHookSo(".*test-memoryhook\\.so$")
                            .addHookSo(".*library-not-exists\\.so$")
                            .enableStacktrace(true)
                            .stacktraceLogThreshold(0)
                    )

                    // Thread hook
                    .addHook(PthreadHook.INSTANCE)
                    .commitHooks();
        } catch (HookManager.HookFailedException e) {
            e.printStackTrace();
        }


        warpFunction(-1);
    }

    public void estimateHash(View view) {
        MemoryHookTestNative.nativeHashCollisionEstimate();
    }

    public void dump(View view) {
        String logPath = getExternalCacheDir().getAbsolutePath() + "/memory-hook.log";
        String jsonPath = getExternalCacheDir().getAbsolutePath() + "/memory-hook.json";
        MemoryHook.INSTANCE.dump(logPath, jsonPath);
    }

    public int warpFunctionImpl(int i, int j) {

        int b = i + 1;

        if (i == j) {
            long start = System.currentTimeMillis();
            MemoryHookTestNative.nativeRunTest();
            long duration = System.currentTimeMillis() - start;

            Log.e(TAG, "Run test duration: " + duration);
        }

        return b;
    }

    public int warpFunction(int i) {

        warpFunctionImpl(i, i);

        return i;
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }

}
