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
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.tencent.components.backtrace.WarmUpReporter;
import com.tencent.components.backtrace.WeChatBacktrace;
import com.tencent.matrix.benchmark.test.UnwindBenchmarkTest;

import java.io.File;
import java.io.FileFilter;
import java.util.HashMap;
import java.util.Map;

public class BenchmarkActivity extends AppCompatActivity {

    private static final String TAG = "Backtrace.Benchmark";

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

        final Button trybtn = findViewById(R.id.btn_wechat_backtrace_try);
        trybtn.setEnabled(false);
        final Button btn = findViewById(R.id.btn_wechat_backtrace_benchmark);
        btn.setEnabled(false);
        final Button jitbtn = findViewById(R.id.btn_wechat_backtrace_benchmark_with_jit);
        jitbtn.setEnabled(false);
        final Button javabtn = findViewById(R.id.btn_wechat_backtrace_benchmark_for_java);
        javabtn.setEnabled(false);

        WeChatBacktrace.setReporter(new WarmUpReporter() {
            @Override
            public void onReport(ReportEvent type, Object... args) {
                if (type == ReportEvent.WarmedUp) {
                    warmedUpToast();
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            trybtn.setEnabled(true);
                            btn.setEnabled(true);
                            jitbtn.setEnabled(true);
                            javabtn.setEnabled(true);
                        }
                    });
                } else if (type == ReportEvent.WarmUpDuration && args.length == 1) {
                    Log.e(TAG, String.format("Warm-up duration: %sms", (long) args[0]));
                }
            }
        });

        // Init backtrace
        WeChatBacktrace.instance().configure(getApplicationContext())
                .warmUpSettings(WeChatBacktrace.WarmUpTiming.PostStartup, 0)
                .directoryToWarmUp(WeChatBacktrace.getSystemFrameworkOATPath() + "boot.oat")
                .directoryToWarmUp(WeChatBacktrace.getSystemFrameworkOATPath() + "boot-framework.oat")
                .enableIsolateProcessLogger(true)
                .enableOtherProcessLogger(false)
                .commit();

        if (WeChatBacktrace.hasWarmedUp(this)) {
            warmedUpToast();
            trybtn.setEnabled(true);
            btn.setEnabled(true);
            jitbtn.setEnabled(true);
            javabtn.setEnabled(true);
        }

        // We need this to refresh maps
        UnwindBenchmarkTest.nativeInit();
    }

    public void backtraceTry(View view) {
        UnwindBenchmarkTest.nativeTry();
    }

    public void backtraceBenchmark(View view) {
        warpFunction(-1);
    }

    public void backtraceBenchmarkForJava(View view) {
        warpFunctionForJava(-1);
    }

    public void backtraceBenchmarkWithJit(View view) {
        warpJitFunction(30000);
    }

    public void warpFunctionImpl2(int i, int j) {
        warpFunctionImpl(i, j);
    }

    public int warpFunctionImpl(int i, int j) {

        int b = i + 1;

        if (i == j) {
            UnwindBenchmarkTest.nativeBenchmark();
        }

        return b;
    }

    public int warpFunctionForJavaImpl(int i, int j) {

        int b = i + 1;

        if (i == j) {
            UnwindBenchmarkTest.nativeBenchmarkJavaStack();
        }

        return b;
    }

    public void javaFunctionImpl(int i, int j) {
        if (i == j) {
            long duration_sum = 0;
            long times = 0;
            Throwable throwable = null;
            for (int t = 0; t < 10000; t++) {
                long start = System.nanoTime();
                throwable = new Throwable();
                long end = System.nanoTime();
                long duration = System.nanoTime() - start;
                duration_sum += duration;
                times++;
                Log.e("Unwind-test", String.format(
                        "Java fillInStackTrace %s(ns) - %s(ns) = costs: %s(ns)", end, start, duration));
            }
            Log.e("Unwind-test", String.format(
                    "Java fillInStackTrace Accumulated duration = %s, times = %s, avg = %s, per-frame = %s", duration_sum, times,
                    duration_sum / times,
                    (duration_sum / times) / throwable.getStackTrace().length));
        }
    }

    public int warpJitFunction(int i) {

        UnwindBenchmarkTest.nativeInit();

        for (int j = 0; j <= i; j++) {
            warpFunctionImpl2(j, i);
        }

        return i;
    }

    public int warpFunction(int i) {

        warpFunctionImpl(i, i);

        return i;
    }

    public int warpFunctionForJava(int i) {

        UnwindBenchmarkTest.nativeInit();

        warpFunctionForJavaImpl(i, i);

        return i;
    }

    public void testMapsUpdate(View view) {
        UnwindBenchmarkTest.nativeRefreshMaps();
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }

    private volatile boolean mIsStatisticsRunning = false;

    public void doStatistics(View view) {
        if (mIsStatisticsRunning) {
            return;
        }

        mIsStatisticsRunning = true;

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {

                HashMap<String, HashMap<String, int[]>> allResults = new HashMap<>();

                boolean isArm64 = WeChatBacktrace.is64BitRuntime();
                File directoryOfTargets = new File(
                        "/data/local/tmp/backtrace-statistics" + (isArm64 ? "/arm64" : "/arm32"));
                for (File target : directoryOfTargets.listFiles()) {
                    if (target.isDirectory() && !allResults.containsKey(target.getName())) {
                        allResults.put(target.getName(), new HashMap<String, int[]>());
                    }

                    final HashMap<String, int[]> maps = allResults.get(target.getName());
                    recursiveDirectory(target, new FileFilter() {
                        @Override
                        public boolean accept(File file) {
                            if (file.getName().endsWith(".so") || file.getName().endsWith(".oat")
                                    || file.getName().endsWith(".odex")) {
                                Log.e(TAG, "Start statistics -> " + file.getAbsolutePath());
                                int[] result = WeChatBacktrace._DoStatistic(file.getAbsolutePath());
                                maps.put(file.getName(), result);
                                Log.e(TAG,
                                        "Start statistics <- " + file.getAbsolutePath() + " end.");
                            }
                            return false;
                        }
                    });
                }

                long total_entries = 0;
                long total_unsupported_entries = 0;
                long total_bad_entries = 0;
                for (Map.Entry<String, HashMap<String, int[]>> entry : allResults.entrySet()) {
                    long entries = 0;
                    long unsupported_entries = 0;
                    long bad_entries = 0;
                    for (Map.Entry<String, int[]> elfStat : entry.getValue().entrySet()) {
                        int entries_counting = 0;
                        int unsupported_entries_counting = 0;
                        int bad_entries_counting = 0;
                        for (int i = 0; i < elfStat.getValue().length; i += 2) {
                            int type = elfStat.getValue()[i];
                            int value = elfStat.getValue()[i + 1];
                            if (type >= 20 && type <= 22) {
                                entries_counting += value;
                            } else if (type >= 33 && type <= 39) {
                                unsupported_entries_counting += value;
                            } else if (type >= 1 && type <= 9) {
                                bad_entries_counting += value;
                            }
                        }
                        Log.e(TAG, String.format("Single.ELF: %s -> %s/%s/%s ", elfStat.getKey(), bad_entries_counting, unsupported_entries_counting, entries_counting));

                        entries += entries_counting;
                        unsupported_entries += unsupported_entries_counting;
                        bad_entries += bad_entries_counting;
                    }
                    Log.e(TAG, String.format("Group.ELF: %s -> %s/%s/%s ", entry.getKey(), bad_entries, unsupported_entries, entries));

                    total_entries += entries;
                    total_unsupported_entries += unsupported_entries;
                    total_bad_entries += bad_entries;
                }

                Log.e(TAG, String.format("Total.ELF: %s/%s/%s ", total_bad_entries, total_unsupported_entries, total_entries));

                mIsStatisticsRunning = false;
            }
        });

        thread.start();
    }

    private void recursiveDirectory(File dir, FileFilter filter) {
        if (dir.isDirectory()) {
            for (File file : dir.listFiles()) {
                recursiveDirectory(file, filter);
            }
        } else {
            filter.accept(dir);
        }
    }

}
