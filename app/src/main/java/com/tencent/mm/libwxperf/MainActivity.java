package com.tencent.mm.libwxperf;

import android.os.Bundle;
import android.os.Process;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;

import com.tencent.mm.performance.jni.fd.FDDumpBridge;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        dumpFd();
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
}
