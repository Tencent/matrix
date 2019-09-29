package com.tencent.mm.libwxperf;

import android.Manifest;
import android.content.pm.PackageManager;
import android.nfc.Tag;
import android.os.Bundle;
import android.os.Environment;
import android.os.Process;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;

import com.tencent.mm.performance.jni.fd.FDDumpBridge;
import com.tencent.mm.performance.jni.memory.MemoryHook;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

//        MemoryHook.hook();
//        MemoryHook.groupByMemorySize(true);
        MemoryHook.INSTANCE
                .addHookSo(".*libnative-lib\\.so$")
                .enableStacktrace(false)
                .hook();

        Log.d(TAG, " pid = " + Process.myPid() + " tid = " + Thread.currentThread().getId());

        findViewById(R.id.button).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final JNIObj jniObj = new JNIObj();
                for (int i = 0; i < 1000; i++) {
                    jniObj.doSomeThing();
                }
//                jniObj.nullptr(null);
//                jniObj.dump(null);
                MemoryHook.INSTANCE.dump("/sdcard/memory_hook.log");
            }
        });

        findViewById(R.id.button2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                jniObj.
//                MemoryHook.dump();
            }
        });

        checkPermission();
//        if (checkPermission()) {
            Log.d(TAG, "onCreate: path = " + Environment.getExternalStorageDirectory().getAbsolutePath());
//        }

//        dumpFd();
    }

    private void dumpFd() {

        long begin = System.currentTimeMillis();
        File fdDir = new File(String.format("/proc/%s/fd", Process.myPid()));
        if (!fdDir.exists()) {
            return;
        }

        try {
            for (File file : fdDir.listFiles()) {
                Log.d(TAG, "onCreate: path = " + file.getAbsolutePath() + " file = "+ FDDumpBridge.getFdPathName(file.getAbsolutePath()));
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
}
