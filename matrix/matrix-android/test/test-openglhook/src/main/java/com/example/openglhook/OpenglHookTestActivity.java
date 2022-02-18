package com.example.openglhook;

import static android.opengl.GLES20.GL_RENDERBUFFER;
import static android.opengl.GLES30.GL_DEPTH24_STENCIL8;
import static android.opengl.GLES30.GL_DEPTH_COMPONENT32F;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Application;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.opengl.GLES20;
import android.opengl.GLUtils;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Process;
import android.util.Log;
import android.view.View;

import com.example.test_openglleak.R;
import com.tencent.matrix.Matrix;
import com.tencent.matrix.openglleak.OpenglLeakPlugin;
import com.tencent.matrix.openglleak.hook.OpenGLHook;
import com.tencent.matrix.openglleak.statistics.LeakMonitorForBackstage;
import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.EGLHelper;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;

public class OpenglHookTestActivity extends AppCompatActivity {

    private static final int JAVA_THREAD_COUNT = 4;
    private static final int NATIVE_THREAD_COUNT = 4;

    private static final String TAG = "matrix.openglHook";

    private final Handler[] mHandlers = new Handler[JAVA_THREAD_COUNT];

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_opengl_leak_test);

        System.loadLibrary("test-openglhook");

        for (int i = 0; i < JAVA_THREAD_COUNT; i++) {
            HandlerThread handlerThread = new HandlerThread("Java_thread_" + i);
            handlerThread.start();
            Handler handler = new Handler(handlerThread.getLooper());
            mHandlers[i] = handler;
        }


        findViewById(R.id.install_opengl_plugin).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Log.e(TAG, "current threadId = " + Thread.currentThread().getName());
                installOpenGLPlugin();
            }
        });

        findViewById(R.id.init_egl_env).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                initEGLContext();
            }
        });

        findViewById(R.id.use_textures).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                useTextures();
            }
        });

        findViewById(R.id.use_buffer).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                useBuffers();
            }
        });

        findViewById(R.id.use_render_buffer).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                useRenderbuffer();
            }
        });

        findViewById(R.id.dump_to_string).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int queueSize = OpenGLHook.getInstance().getResidualQueueSize();
                Log.e(TAG, "queueSize = " + queueSize);

                if (queueSize != 0) {
                    return;
                }

                String content = ResRecordManager.getInstance().dumpGLToString();
                Log.e(TAG, "dump content = " + content);
            }
        });

        findViewById(R.id.dump_to_file).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int queueSize = OpenGLHook.getInstance().getResidualQueueSize();
                Log.e(TAG, "queueSize = " + queueSize);

                if (queueSize != 0) {
                    return;
                }

                openglDump();
            }
        });

        findViewById(R.id.gl_profiler).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openglNativeProfiler(NATIVE_THREAD_COUNT);
                openglJavaProfiler();
            }
        });

    }

    private native void openglNativeProfiler(int threadCnt);

    private void openglJavaProfiler() {
        for (int i = 0; i < JAVA_THREAD_COUNT; i++) {

            mHandlers[i].post(new Runnable() {
                @Override
                public void run() {
                    EGLHelper.initOpenGL();
                    long start = System.currentTimeMillis();

                    int totalCount = 5000;
                    int[] textures = new int[totalCount];
                    int[] buffers = new int[totalCount];
                    int[] renderbuffers = new int[totalCount];

                    for (int i = 0; i < totalCount; i++) {

                        GLES20.glGenRenderbuffers(1, renderbuffers, i);
                        GLES20.glGenTextures(1, textures, i);
                        GLES20.glGenBuffers(1, buffers, i);

                        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, buffers[i]);
                        GLES20.glBufferData(GLES20.GL_ARRAY_BUFFER, 1, null, GLES20.GL_STATIC_DRAW);
                        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textures[i]);

                        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, 1, 1, 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
                        GLES20.glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[i]);
                        GLES20.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 0, 0);
                    }
                    long end = System.currentTimeMillis();
                    long cost = end - start;
                    Log.e(TAG, Thread.currentThread().getName() + ": openglProfiler finish, cost = " + cost);
                }
            });
        }
    }

    private void openglDump() {
        final String dumpPath = getExternalCacheDir() + File.separator + Process.myPid() + "_opengl_hook.log";

        File dumpFile = new File(dumpPath);
        if (dumpFile.exists()) {
            boolean result = dumpFile.delete();
            Log.e(TAG, "delete dumpFile result = " + result);
        }

        try {
            dumpFile.createNewFile();
        } catch (IOException e) {
            Log.e(TAG, "dumpFile create fail");
        }

        ResRecordManager.getInstance().dumpGLToFile(dumpPath);
        Log.e(TAG, "openGLHook dump, dumpPath = " + dumpPath);
    }

    private void useRenderbuffer() {
        int[] renderbuffers = new int[1];
        GLES20.glGenRenderbuffers(1, IntBuffer.wrap(renderbuffers));
        GLES20.glBindRenderbuffer(GL_RENDERBUFFER, renderbuffers[0]);
        GLES20.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 3000, 3000);
        GLES20.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 10480, 10480);
        GLES20.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 10480, 10480);
        GLES20.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 10250, 10250);
    }

    private void useBuffers() {
        int[] buffers = new int[1];

        ByteBuffer vertexBuffer = ByteBuffer.allocateDirect(40 * 15);
        vertexBuffer.putInt(1);
        vertexBuffer.putInt(2);
        vertexBuffer.putInt(3);
        vertexBuffer.putInt(4);

        GLES20.glGenBuffers(1, IntBuffer.wrap(buffers));
        GLES20.glBindBuffer(GLES20.GL_ARRAY_BUFFER, buffers[0]);
        GLES20.glBufferData(GLES20.GL_ARRAY_BUFFER, 40, vertexBuffer, GLES20.GL_STATIC_DRAW);
    }

    private void useTextures() {
        Bitmap bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.ic_launcher);

        int[] textures = new int[1];
        GLES20.glGenTextures(1, IntBuffer.wrap(textures));
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textures[0]);
        GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
    }

    private void initEGLContext() {
        EGLHelper.initOpenGL();
    }


    private void installOpenGLPlugin() {
        OpenglLeakPlugin plugin = new OpenglLeakPlugin(getApplication());
        new Matrix.Builder(((Application) getApplicationContext())).plugin(plugin).build().startAllPlugins();
        LeakMonitorForBackstage monitor = new LeakMonitorForBackstage(3 * 1000L);
        monitor.setLeakListener(new LeakMonitorForBackstage.LeakListener() {
            @Override
            public void onLeak(OpenGLInfo info) {
                Log.e("onLeak", info + "");
            }
        });
        monitor.start((Application) getApplicationContext());

//        OpenGLHook.getInstance().setResourceListener(new OpenGLHook.ResourceListener() {
//            @Override
//            public void onGlGenTextures(OpenGLInfo info) {
//                Log.e(TAG, "onGlGenTextures = " + info);
//            }
//
//            @Override
//            public void onGlDeleteTextures(OpenGLInfo info) {
//                Log.e(TAG, "onGlDeleteTextures = " + info);
//            }
//
//            @Override
//            public void onGlGenBuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlGenBuffers = " + info);
//            }
//
//            @Override
//            public void onGlDeleteBuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlDeleteBuffers = " + info);
//            }
//
//            @Override
//            public void onGlGenFramebuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlGenFramebuffers = " + info);
//            }
//
//            @Override
//            public void onGlDeleteFramebuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlDeleteFramebuffers = " + info);
//            }
//
//            @Override
//            public void onGlGenRenderbuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlGenRenderbuffers = " + info);
//            }
//
//            @Override
//            public void onGlDeleteRenderbuffers(OpenGLInfo info) {
//                Log.e(TAG, "onGlDeleteRenderbuffers = " + info);
//            }
//        });
//
//        OpenGLHook.getInstance().setMemoryListener(new OpenGLHook.MemoryListener() {
//            @Override
//            public void onGlTexImage2D(int target, int level, int internalFormat, int width, int height, int border, int format, int type, int id, long eglContextId, long size) {
//                Log.e(TAG, "onGlTexImage2D , target = " + target + "， level = " + level
//                        + ", internalFormat = " + internalFormat + ", width = " + width + ", height = " + height
//                        + ", border = " + border + ", format = " + format + ", type = " + type + ", id = " + id
//                        + ". eglContextId = " + eglContextId + ", size = " + size);
//            }
//
//            @Override
//            public void onGlTexImage3D(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size) {
//                Log.e(TAG, "onGlTexImage3D , target = " + target + "， level = " + level
//                        + ", internalFormat = " + internalFormat + ", width = " + width + ", height = " + height
//                        + ", depth = " + depth + ", border = " + border + ", format = " + format
//                        + ", type = " + type + ", id = " + id + ". eglContextId = " + eglContextId + ", size = " + size);
//            }
//
//            @Override
//            public void onGlBufferData(int target, int usage, int id, long eglContextId, long size) {
//                Log.e(TAG, "onGlBufferData, target = " + target + ", usage = " + usage + ", id = " + id
//                        + ", eglContextId = " + eglContextId + ", size = " + size);
//            }
//
//            @Override
//            public void onGlRenderbufferStorage(int target, int width, int height, int internalFormat, int id, long eglContextId, long size) {
//                Log.e(TAG, "onGlRenderbufferStorage, target = " + target + ", width = " + width
//                        + ", height = " + height + ", internalFormat = " + internalFormat + ", id = " + id
//                        + " , eglContextId = " + eglContextId + ", size = " + size);
//            }
//        });
//
//        OpenGLHook.getInstance().setBindListener(new OpenGLHook.BindListener() {
//            @Override
//            public void onGlBindTexture(int target, long eglContextId, int id) {
//                Log.e(TAG, "onGlBindTexture, target = " + target + ", eglContextId = " + eglContextId + ", id = " + id);
//            }
//
//            @Override
//            public void onGlBindBuffer(int target, long eglContextId, int id) {
//                Log.e(TAG, "onGlBindBuffer, target = " + target + ", eglContextId = " + eglContextId + ", id = " + id);
//            }
//
//            @Override
//            public void onGlBindRenderbuffer(int target, long eglContextId, int id) {
//                Log.e(TAG, "onGlBindRenderbuffer, target = " + target + ", eglContextId = " + eglContextId + ", id = " + id);
//            }
//
//            @Override
//            public void onGlBindFramebuffer(int target, long eglContextId, int id) {
//                Log.e(TAG, "onGlBindFramebuffer, target = " + target + ", eglContextId = " + eglContextId + ", id = " + id);
//            }
//        });
    }

}