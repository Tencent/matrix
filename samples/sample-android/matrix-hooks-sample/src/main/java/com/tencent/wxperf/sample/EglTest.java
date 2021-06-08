package com.tencent.wxperf.sample;

import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;

public class EglTest {

    static EGLDisplay mEGLDisplay;
    static EGLContext mEglContext;
    static EGLConfig mEglConfig;
    static EGLSurface mEglSurface;

    public static void initOpenGL() {
        mEGLDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (mEGLDisplay == EGL14.EGL_NO_DISPLAY) {
            return;
        }

        int[] version = new int[2];
        if (!EGL14.eglInitialize(mEGLDisplay, version, 0, version, 1)) {
            return;
        }

        int[] eglConfigAttribList = new int[]{
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_NONE
        };
        int[] numEglConfigs = new int[1];
        EGLConfig[] eglConfigs = new EGLConfig[1];
        if (!EGL14.eglChooseConfig(mEGLDisplay, eglConfigAttribList, 0, eglConfigs, 0,
                eglConfigs.length, numEglConfigs, 0)) {
            return;
        }
        mEglConfig = eglConfigs[0];

        int[] eglContextAttribList = new int[]{
                EGL14.EGL_CONTEXT_CLIENT_VERSION, 2,
                EGL14.EGL_NONE
        };

        mEglContext = EGL14.eglCreateContext(mEGLDisplay, mEglConfig, EGL14.EGL_NO_CONTEXT,
                eglContextAttribList, 0);
        if (mEglContext == EGL14.EGL_NO_CONTEXT) {
            return;
        }

        int[] surfaceAttribList = {
                EGL14.EGL_WIDTH, 64,
                EGL14.EGL_HEIGHT, 64,
                EGL14.EGL_NONE
        };

        // Java线程不进行实际绘制，因此创建PbufferSurface而非WindowSurface
        mEglSurface = EGL14.eglCreatePbufferSurface(mEGLDisplay, mEglConfig, surfaceAttribList, 0);
        if (mEglSurface == EGL14.EGL_NO_SURFACE) {
            return;
        }

        if (!EGL14.eglMakeCurrent(mEGLDisplay, mEglSurface, mEglSurface, mEglContext)) {
            return;
        }

        GLES20.glFlush();
    }

    public static void release() {
        EGL14.eglDestroySurface(mEGLDisplay, mEglSurface);
        EGL14.eglDestroyContext(mEGLDisplay, mEglContext);
        EGL14.eglReleaseThread();
        EGL14.eglTerminate(mEGLDisplay);
    }
}
