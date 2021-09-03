package com.tencent.wxperf.sample;

import androidx.appcompat.app.AppCompatActivity;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES10;
import android.opengl.GLES20;
import android.opengl.GLES32;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Process;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;

import com.tencent.matrix.hook.HookManager;
import com.tencent.matrix.hook.memory.MemoryHook;
import com.tencent.matrix.openglleak.utils.EGLHelper;
import com.tencent.matrix.util.MatrixLog;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.IntBuffer;

public class OpenglLeakActivity extends AppCompatActivity {

    private static final String TAG = "Backtrace.Benchmark";

    private int[] mTextures = new int[1];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_opengl_leak);
        Log.e(TAG, "OpenglLeakActivity onCreate success call");

//        dump();

        Log.e(TAG, "\n------------------------------------------------------------------------------------------------------------------------------------\n");

        Log.e(TAG, "make openGL resource start");
        EGLHelper.initOpenGL();
        Log.e(TAG, "make openGL resource end");

        Log.e(TAG, "\n------------------------------------------------------------------------------------------------------------------------------------\n");

//        dump();

        findViewById(R.id.button_finish_activity).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.e(TAG, "click button_finish_activity & do finish");
                finish();
            }
        });

//        OpenglRender render = new OpenglRender(this, mTextures[0]);
//        render.loadTexture();

        FrameLayout layout = findViewById(R.id.frame_layout_surface_view);
        GLSurfaceView glView = new GLSurfaceView(this);
        glView.setEGLContextClientVersion(2);
        OpenglRender render = new OpenglRender(this, mTextures[0]);
        glView.setRenderer(render);
        layout.addView(glView);

//        HookManager.INSTANCE.dumpToLog(TAG);


//        File fileDir = new File(LOG_FILE_DIR);
//        fileDir.mkdir();
//        File logFile = new File(LOG_FILE_PATH);
//        if (logFile.exists()) {
//            logFile.delete();
//        }
//        HookManager.INSTANCE.dumpToFile(logFile.getAbsolutePath());
//
//        readFile(logFile.getAbsolutePath());

//        HookManager.INSTANCE.clearList();
    }

    @Override
    protected void onResume() {
        super.onResume();
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                dump("test");
            }
        }, 10000);

    }

    @Override
    protected void onDestroy() {
        dump("OpenglLeak");
        EGLHelper.release();
        super.onDestroy();
    }

    private void dump(String lifeCycle) {

        @SuppressLint("SdCardPath") final String logFilePath =
                TestApp.getContext().getExternalCacheDir() + File.separator + Process.myPid() + "_" + lifeCycle + "_gpu_memory_hook.log";

        @SuppressLint("SdCardPath") final String jsonFilePath =
                TestApp.getContext().getExternalCacheDir() + File.separator + Process.myPid() + "_" + lifeCycle + "_gpu_memory_hook.json";

        File logFile = new File(logFilePath);
        File jsonFile = new File(jsonFilePath);

        if (logFile.exists()) {
            boolean result = logFile.delete();
            Log.e(TAG, "delete logFile result = " + result);
        }
        try {
            logFile.createNewFile();
        } catch (IOException e) {
            Log.e(TAG, "logFile create fail");
        }

        if (jsonFile.exists()) {
            boolean result = jsonFile.delete();
            Log.e(TAG, "delete jsonFile result = " + result);
        }
        try {
            jsonFile.createNewFile();
        } catch (IOException e) {
            Log.e(TAG, "jsonFile create fail");
        }

        MemoryHook.INSTANCE.dump(logFile.getAbsolutePath(), jsonFile.getAbsolutePath());
        Log.e(TAG, logFile.getAbsolutePath());
    }


}