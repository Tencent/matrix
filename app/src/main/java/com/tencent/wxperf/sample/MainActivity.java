package com.tencent.wxperf.sample;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;

import com.tencent.wxperf.fd.FDDumpBridge;
import com.tencent.wxperf.jectl.JeCtl;
import com.tencent.wxperf.jni.HookManager;
import com.tencent.wxperf.jni.egl.EglHook;
import com.tencent.wxperf.jni.memory.MemoryHook;
import com.tencent.wxperf.jni.pthread.PthreadHook;
import com.tencent.wxperf.jni.test.UnwindBenckmarkTest;
import com.tencent.wxperf.jni.test.UnwindTest;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "Wxperf.MainActivity";

    String threadNameRegex = "[GT]TestHT-?".replace("[", "\\[").replace("]", "\\]").replace("?", "[0-9]*");

    String name = "[GT]TestHT-12";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

//        {
//            try {
//                Class.forName("com.tencent.wxperf.jni.HookManager");
//            } catch (ClassNotFoundException e) {
//                e.printStackTrace();
//            }
//        }

//        LibWxPerfManager.INSTANCE.init();
//        if (!LibWxPerfManager.INSTANCE.initOk()) {
//            throw new RuntimeException("init failed");
//        }
//
//        Log.d(TAG, "threadName = " + threadNameRegex + ", " + name.matches(threadNameRegex));
//
//        try {
//            HookManager.INSTANCE
//                    .addHook(MemoryHook.INSTANCE
////                            .addHookSo(".*libnative-lib\\.so$")
//                            .addHookSo(".*libnative-lib\\.so$")
//                            .enableStacktrace(true)
//                            .enableMmapHook(false))
//                    .addHook(PthreadHook.INSTANCE
////                            .addHookSo(".*libnative-lib\\.so$")
//                                    .addHookSo(".*\\.so$")
////                            .addIgnoreSo(".*libart\\.so$")
//                                    .addHookThread(".*")
////                                    .addHookThread(threadNameRegex)
////                            .addHookThread("MyHandlerThread")
////                            .addHookThread("\\[GT\\]MediaCodecR$")
//                    )
//                    .commitHooks();
//
////            throw new HookManager.HookFailedException("adfad");
//        } catch (HookManager.HookFailedException e) {
//            e.printStackTrace();
//        }
//
//        UnwindTest.init();
//        UnwindBenckmarkTest.benchmarkInitNative();
//
        checkPermission();
//        Log.d(TAG, "onCreate: path = " + Environment.getExternalStorageDirectory().getAbsolutePath());

        EglHook.initILog(new EglHook.ILog() {
            @Override
            public void v(String tag, String info) {

            }

            @Override
            public void i(String tag, String info) {

            }

            @Override
            public void e(String tag, String info) {
                Log.e(tag, info);
            }

            @Override
            public void w(String tag, String info) {

            }

            @Override
            public void d(String tag, String info) {

            }
        });

        findViewById(R.id.btn_egl).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    HookManager.INSTANCE
                            .addHook(EglHook.INSTANCE).commitHooks();
                    Log.e("Cc1over-debug", "hook success");
                } catch (HookManager.HookFailedException e) {
                    Log.e("Cc1over-debug", "hook fail");
                }
            }
        });

        findViewById(R.id.btn_egl_create_context).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                EglTest.initOpenGL();
            }
        });

        findViewById(R.id.btn_egl_destroy_context).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                EglTest.release();
            }
        });

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
//                    jniObj.reallocTest();
//                    jniObj.reallocTest();
                }
            }).start();
        }
//        new Handler().postDelayed(new Runnable() {
//            @Override
//            public void run() {
//                MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log");
//            }
//        }, 2000);

        try {
            Thread.sleep(500);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log");
    }

    public void mmapTest(View view) {
        JNIObj jniObj = new JNIObj();
        jniObj.doMmap();
        MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log");
    }

    public void threadTest(View view) {

        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {

                for (int i = 0; i < 100; i++) {
                    Log.d("Yves-debug", "test thread " + i);
                    JNIObj.testThread();

                    HandlerThread ht = new HandlerThread("[GT]TestHT-" + i);
                    ht.start();

                    new Handler(ht.getLooper()).post(new Runnable() {
                        @Override
                        public void run() {

                            new Thread(new Runnable() {
                                @Override
                                public void run() {
//                                    try {
//                                        Thread.sleep(1000);
//                                    } catch (InterruptedException e) {
//                                        e.printStackTrace();
//                                    }
                                }
                            }, "SubTestTh").start();


                            new Thread(new Runnable() {
                                @Override
                                public void run() {

                                }
                            }).start();

                            new Thread(new Runnable() {
                                @Override
                                public void run() {

                                }
                            }).start();

                            new Thread(new Runnable() {
                                @Override
                                public void run() {

                                }
                            }).start();

                            try {
                                Thread.sleep(3000);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                    });
                }

            }
        }, "[GT]HotPool#1");

        t.start();

        HandlerThread handlerThread = new HandlerThread("HandlerThread1");
        handlerThread.start();

        try {
            Thread.sleep(10 * 1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        PthreadHook.INSTANCE.dump("/sdcard/pthread_hook.log");
    }


    public void mallocTest(View view) {
        Log.d(TAG, "mallocTest: native heap:" + Debug.getNativeHeapSize() + ", allocated:" + Debug.getNativeHeapAllocatedSize() + ", free:" + Debug.getNativeHeapFreeSize());
        JNIObj.mallocTest();
        Log.d(TAG, "mallocTest after malloc: native heap:" + Debug.getNativeHeapSize() + ", allocated:" + Debug.getNativeHeapAllocatedSize() + ", free:" + Debug.getNativeHeapFreeSize());
    }

    public void doSomeThing(View view) {
        final JNIObj jniObj = new JNIObj();
        for (int i = 0; i < 1000; i++) {
            jniObj.doSomeThing();
        }
        MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log");
    }

    public void specificTest(View view) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < 100; i++) {
                    JNIObj.testThreadSpecific();
                }
            }
        }).start();
    }

    public void concurrentJNITest(View view) {
        for (int i = 0; i < 100; i++) {

            new Thread(new Runnable() {
                @Override
                public void run() {
                    JNIObj.testJNICall();
                    JNIObj.testJNICall();
                }
            }).start();
        }

//        JNIObj.testPthreadFree();

        JNIObj.testJNICall();
    }

    public void unwindTest(View view) {

        new Thread(new Runnable() {
            @Override
            public void run() {
                for (int i = 0; i < 1; i++) {
//                            long begin = System.nanoTime();
                    UnwindTest.test();
//                            Log.i("Unwind-test", "unwind cost: " + (System.nanoTime() - begin));
                }

                PthreadHook.INSTANCE.dump("/sdcard/pthread_hook.log");
            }
        }).start();
    }

    public void unwindBenchmark(View view) {

        new Thread(new Runnable() {
            @Override
            public void run() {
                UnwindBenckmarkTest.benchmarkNative();
            }
        }).start();
    }

    public void unwindBenchmarkDebug(View view) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                UnwindBenckmarkTest.debugNative();
            }
        }).start();
    }

    public void memoryBenchmark(View view) {
//        MemoryBenchmarkTest.benchmarkNative();
        try {
            HookManager.INSTANCE
                    .addHook(MemoryHook.INSTANCE
                            .addHookSo(".*libnative-lib\\.so$")
                            .enableStacktrace(false)
                            .enableMmapHook(false))
                    .commitHooks();
        } catch (HookManager.HookFailedException e) {
            e.printStackTrace();
        }
        MemoryBenchmarkTest.benchmarkNative();
    }

    public void tlsTest(View view) {
        JNIObj.tlsTest();
    }

    public void poolTest(View view) {
        for (int i = 0; i < 10; i++) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    JNIObj.poolTest();
                }
            }).start();
        }
    }

    public void epollTest(View view) {
//        JNIObj.epollTest();
//
//        try {
//            Thread.sleep(1000);
//        } catch (InterruptedException e) {
//            e.printStackTrace();
//        }

        for (int i = 0; i < 1; i++) {

            new Thread(new Runnable() {
                @Override
                public void run() {
                    JNIObj.epollTest();
                }
            }).start();
        }
    }

    public void concurrentMapTest(View view) {
        JNIObj.concurrentMapTest();
    }

    public void jectlTest(View view) {
//        JeCtl.extentHookTest();
        JeCtl.preAllocRetain(500 * 1024 * 1024 /*up to 640M*/, 120 * 1024 * 1024 /*up to 128M*/, 256 * 1024 * 1024,128 * 1024 * 1024);
//        int ret = JeCtl.compact();
//        Log.d(TAG, "tryDisableRetain result :" + ret);
    }

    public void killSelf(View view) {
        Process.killProcess(Process.myPid());
    }
}
