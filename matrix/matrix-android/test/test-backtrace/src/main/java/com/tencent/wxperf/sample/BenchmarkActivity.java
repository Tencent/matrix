package com.tencent.wxperf.sample;

import android.Manifest;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.tencent.components.backtrace.WarmUpReporter;
import com.tencent.components.backtrace.WeChatBacktrace;
import com.tencent.matrix.benchmark.test.UnwindBenchmarkTest;

public class BenchmarkActivity extends AppCompatActivity {

    private static final String TAG = "Backtrace.BenchmarkActivity";

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
                Toast.makeText(BenchmarkActivity.this, "Warm-up has been done!", Toast.LENGTH_LONG).show();
            }
        });

    }

    private boolean mBacktraceTestInitialized = false;

    private Handler mHandler = new Handler(Looper.getMainLooper());

    public void backtraceInit() {

        if (mBacktraceTestInitialized) {
            return;
        }

        mBacktraceTestInitialized = true;


        if (WeChatBacktrace.hasWarmedUp(this)) {
            warmedUpToast();
        }

        WeChatBacktrace.setReporter(new WarmUpReporter() {
            @Override
            public void onReport(ReportEvent type, Object... args) {
                if (type == ReportEvent.WarmedUp) {
                    warmedUpToast();
                }
            }
        });

        // Init backtrace
        WeChatBacktrace.instance().configure(getApplicationContext())
                .warmUpSettings(WeChatBacktrace.WarmUpTiming.PostStartup, 0)
                .commit();

    }

    public void backtraceBenchmark(View view) {
        UnwindBenchmarkTest.nativeInit();
        UnwindBenchmarkTest.nativeBenchmark();
    }

    public void backtrace(View view) {
        UnwindBenchmarkTest.nativeTry();
    }

    public void testMapsUpdate(View view) {
        UnwindBenchmarkTest.nativeRefreshMaps();
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }
}
