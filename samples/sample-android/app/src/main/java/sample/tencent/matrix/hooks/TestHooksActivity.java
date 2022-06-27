package sample.tencent.matrix.hooks;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;
import android.view.View;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.tencent.matrix.backtrace.WarmUpReporter;
import com.tencent.matrix.backtrace.WeChatBacktrace;
import com.tencent.matrix.fd.FDDumpBridge;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.hook.memory.MemoryHook;
import com.tencent.matrix.hook.pthread.PthreadHook;
import com.tencent.matrix.jectl.JeCtl;

import java.io.File;

import sample.tencent.matrix.R;

public class TestHooksActivity extends Activity {

    private static final String TAG = "Matrix.Hooks";

    String threadNameRegex = "[GT]TestHT-?".replace("[", "\\[").replace("]", "\\]").replace("?", "[0-9]*");

    String name = "[GT]TestHT-12";

    private static boolean sInitiated = false;

    public static boolean is64BitRuntime() {
        final String currRuntimeABI = Build.CPU_ABI;
        return "arm64-v8a".equalsIgnoreCase(currRuntimeABI)
                || "x86_64".equalsIgnoreCase(currRuntimeABI)
                || "mips64".equalsIgnoreCase(currRuntimeABI);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_hooks);

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

        if (!sInitiated) {
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

            // Init Hooks.
            try {

                PthreadHook.INSTANCE
                        .addHookThread(".*")
                        .setThreadTraceEnabled(true)
                        .enableTracePthreadRelease(true)
                        .enableQuicken(false);

                PthreadHook.INSTANCE.enableLogger(false);

                HookManager.INSTANCE

                        // Memory hook
                        .addHook(MemoryHook.INSTANCE
                                .addHookSo(".*libnative-lib\\.so$")
                                .enableStacktrace(true)
                                .stacktraceLogThreshold(0)
                                .enableMmapHook(true)
                        )

                        // Thread hook
                        .addHook(PthreadHook.INSTANCE)
                        .commitHooks();
            } catch (HookManager.HookFailedException e) {
                e.printStackTrace();
            }
        }


        checkPermission();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    private void dumpFd() {

        long begin = System.currentTimeMillis();
        File fdDir = new File(String.format("/proc/%s/fd", Process.myPid()));
        if (!fdDir.exists()) {
            return;
        }

        try {
            for (File file : fdDir.listFiles()) {
                Log.d(TAG, "onCreate: path = " + file.getAbsolutePath() + " file = "
                        + FDDumpBridge.getFdPathName(file.getAbsolutePath()));
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        Log.d(TAG, "dump cost:" + (System.currentTimeMillis() - begin));
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

    public void memoryAllocTest(View view) {

        // mmap
        final JNIObj jniObj = new JNIObj();
        jniObj.doMmap();

        // realloc
        for (int i = 0; i < 30; i++) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    jniObj.reallocTest();
                }
            }).start();
        }
        try {
            Thread.sleep(500);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        // malloc
        Log.d(TAG, "mallocTest: native heap:" + Debug.getNativeHeapSize() + ", allocated:"
                + Debug.getNativeHeapAllocatedSize() + ", free:" + Debug.getNativeHeapFreeSize());
        JNIObj.mallocTest();
        Log.d(TAG,
                "mallocTest after malloc: native heap:" + Debug.getNativeHeapSize() + ", allocated:"
                        + Debug.getNativeHeapAllocatedSize() + ", free:"
                        + Debug.getNativeHeapFreeSize());

        String output = getExternalCacheDir() + "/memory_hook.log";
        MemoryHook.INSTANCE.dump(output, output + ".json");
    }

    public void threadTest(View view) {

        for (int i = 0; i < 50; i++) {
            new HandlerThread("Test").start();
        }

        JNIObj.threadTest();

        try {
            Thread.sleep(5000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        String output = getExternalCacheDir() + "/pthread_hook.log";
        PthreadHook.INSTANCE.dump(output);
    }

    public void jectlTest(View view) {
        Log.d(TAG, "jemalloc version = " + JeCtl.version());
        Log.d(TAG, "set retain, old value = " + JeCtl.setRetain(true));
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }

    public void fdLimit(View view) {
        Log.i(TAG, "FD limit = " + FDDumpBridge.getFDLimit());
    }
}
