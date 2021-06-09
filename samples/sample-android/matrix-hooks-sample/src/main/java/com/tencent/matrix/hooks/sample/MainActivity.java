package com.tencent.matrix.hooks.sample;

import android.Manifest;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;
import android.view.View;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.tencent.components.backtrace.WeChatBacktrace;
import com.tencent.matrix.fd.FDDumpBridge;
import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.hook.memory.MemoryHook;
import com.tencent.matrix.hook.pthread.PthreadHook;
import com.tencent.matrix.jectl.JeCtl;

import java.io.File;

public class MainActivity extends Activity {

    private static final String TAG = "Wxperf.Main";

    String threadNameRegex = "[GT]TestHT-?".replace("[", "\\[").replace("]", "\\]").replace("?", "[0-9]*");

    String name = "[GT]TestHT-12";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

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

        // Init backtrace

//        if (IS_ARM64) {
            WeChatBacktrace.instance()
                    .configure(getApplication())
                    .setBacktraceMode(WeChatBacktrace.Mode.Fp)
                    .commit();
//        } else {
//
//            String additionalLibsDir = "/path/to/libs/";
//
//            // run before commit hooks for warm up.
//            WeChatBacktrace.instance().configure(getApplicationContext())
//                    .setBacktraceMode(WeChatBacktrace.Mode.FpUntilQuickenWarmedUp)
////                    .directoryToWarmUp(additionalLibsDir) // your additional native library dir like tinker-patch library dir, application libs/system libs/odex are configured by default
////                    .coolDown(versionUpdated) // set true if your app version updated, redo warm up
//                    .warmUpInIsolateProcess(true) // optional, true by default
//                    .enableIsolateProcessLogger(true) // optional
//                    .xLoggerPath(getApplication().getApplicationInfo().nativeLibraryDir + "/libwechatxlog.so") // optional, xlogger work when enableIsolateProcessLogger is true
//                    .commit();
//        }

        try {
            HookManager.INSTANCE

                    // Memory hook
                    .addHook(MemoryHook.INSTANCE
                            .addHookSo(".*libnative-lib\\.so$")
                            .enableStacktrace(true)
                            .stacktraceLogThreshold(0)
                            .enableMmapHook(true))

                    // Thread hook
                    .addHook(PthreadHook.INSTANCE
                                    .addHookSo(".*\\.so$")
                                    .addHookThread(".*")
//                            .addHookSo(".*libnative-lib\\.so$")
//                            .addIgnoreSo(".*libart\\.so$")
//                                    .addHookThread(threadNameRegex)
//                            .addHookThread("MyHandlerThread")
//                            .addHookThread("\\[GT\\]MediaCodecR$")
                    )
                    .commitHooks();
        } catch (HookManager.HookFailedException e) {
            e.printStackTrace();
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
                Log.d(TAG, "onCreate: path = " + file.getAbsolutePath() + " file = " + FDDumpBridge.getFdPathName(file.getAbsolutePath()));
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

    public void reallocTest(View view) {
        final JNIObj jniObj = new JNIObj();
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

        MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log", "/sdcard/memory_hook.json");
    }

    public void mmapTest(View view) {
        JNIObj jniObj = new JNIObj();
        jniObj.doMmap();
        MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log", "/sdcard/memory_hook.json");
    }

    public void threadTest(View view) {

        for (int i = 0; i < 50; i++) {
            new HandlerThread("Test").start();
//            new Thread(new Runnable() {
//                @Override
//                public void run() {
//                    Log.d(TAG, "run");
//                }
//            }).start();
        }

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                PthreadHook.INSTANCE.dump("/sdcard/pthread_hook_test.json");
            }
        }, 5000);

//        Thread t = new Thread(new Runnable() {
//            @Override
//            public void run() {
//
//                for (int i = 0; i < 100; i++) {
//                    Log.d("Yves-debug", "test thread " + i);
//                    JNIObj.testThread();
//
//                    HandlerThread ht = new HandlerThread("[GT]TestHT-" + i);
//                    ht.start();
//
//                    new Handler(ht.getLooper()).post(new Runnable() {
//                        @Override
//                        public void run() {
//
//                            new Thread(new Runnable() {
//                                @Override
//                                public void run() {
////                                    try {
////                                        Thread.sleep(1000);
////                                    } catch (InterruptedException e) {
////                                        e.printStackTrace();
////                                    }
//                                }
//                            }, "SubTestTh").start();
//
//
//                            new Thread(new Runnable() {
//                                @Override
//                                public void run() {
//
//                                }
//                            }).start();
//
//                            new Thread(new Runnable() {
//                                @Override
//                                public void run() {
//
//                                }
//                            }).start();
//
//                            new Thread(new Runnable() {
//                                @Override
//                                public void run() {
//
//                                }
//                            }).start();
//
//                            try {
//                                Thread.sleep(3000);
//                            } catch (InterruptedException e) {
//                                e.printStackTrace();
//                            }
//                        }
//                    });
//                }
//
//            }
//        }, "[GT]HotPool#1");
//
//        t.start();
//
//        HandlerThread handlerThread = new HandlerThread("HandlerThread1");
//        handlerThread.start();
//
//        try {
//            Thread.sleep(10 * 1000);
//        } catch (InterruptedException e) {
//            e.printStackTrace();
//        }
//
//        PthreadHook.INSTANCE.dump("/sdcard/pthread_hook.log");
    }


    public void mallocTest(View view) {
        Log.d(TAG, "mallocTest: native heap:" + Debug.getNativeHeapSize() + ", allocated:" + Debug.getNativeHeapAllocatedSize() + ", free:" + Debug.getNativeHeapFreeSize());
        JNIObj.mallocTest();
        Log.d(TAG, "mallocTest after malloc: native heap:" + Debug.getNativeHeapSize() + ", allocated:" + Debug.getNativeHeapAllocatedSize() + ", free:" + Debug.getNativeHeapFreeSize());
    }

    public void jectlTest(View view) {
//        JeCtl.extentHookTest();
//        JeCtl.preAllocRetain(320 * 1024 * 1024 /*up to 384M*/, 60 * 1024 * 1024 /*up to 64M*/, 256 * 1024 * 1024,64 * 1024 * 1024);
//        int ret = JeCtl.compact();
//        Log.d(TAG, "tryDisableRetain result :" + ret);
        Log.d(TAG, "jemalloc version = " + JeCtl.version());
        Log.d(TAG, "set retain, old value = " +  JeCtl.setRetain(true));
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }

    public void fdLimit(View view) {
        Log.d(TAG, "FD limit = " + FDDumpBridge.getFDLimit());
    }
}
