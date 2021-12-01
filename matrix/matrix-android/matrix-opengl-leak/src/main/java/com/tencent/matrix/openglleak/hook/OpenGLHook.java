package com.tencent.matrix.openglleak.hook;

import android.opengl.EGL14;

import com.tencent.matrix.openglleak.comm.FuncNameString;
import com.tencent.matrix.openglleak.statistics.BindCenter;
import com.tencent.matrix.openglleak.statistics.LeakMonitor;
import com.tencent.matrix.openglleak.statistics.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.OpenGLResRecorder;
import com.tencent.matrix.util.MatrixLog;

import java.util.concurrent.atomic.AtomicInteger;

import static com.tencent.matrix.openglleak.statistics.OpenGLInfo.OP_GEN;
import static com.tencent.matrix.openglleak.statistics.OpenGLInfo.OP_DELETE;
import static com.tencent.matrix.openglleak.statistics.OpenGLInfo.OP_BIND;

public class OpenGLHook {

    static {
        System.loadLibrary("matrix-opengl-leak");
    }

    private static final String TAG = "MicroMsg.OpenGLHook";

    private static final OpenGLHook mInstance = new OpenGLHook();
    private ResourceListener mResourceListener;
    private ErrorListener mErrorListener;
    private BindListener mBindListener;
    private MemoryListener mMemoryListener;

    private OpenGLHook() {
    }

    public static OpenGLHook getInstance() {
        return mInstance;
    }

    public void setResourceListener(ResourceListener listener) {
        mResourceListener = listener;
    }

    public void setErrorListener(ErrorListener listener) {
        mErrorListener = listener;
    }

    public void setBindListener(BindListener listener) {
        mBindListener = listener;
    }

    public void setMemoryListener(MemoryListener listener) {
        mMemoryListener = listener;
    }

    public boolean hook(String targetFuncName, int index) {
        switch (targetFuncName) {
            case FuncNameString.GL_GET_ERROR:
                return hookGlGetError(index);
            case FuncNameString.GL_GEN_TEXTURES:
                return hookGlGenTextures(index);
            case FuncNameString.GL_DELETE_TEXTURES:
                return hookGlDeleteTextures(index);
            case FuncNameString.GL_GEN_BUFFERS:
                return hookGlGenBuffers(index);
            case FuncNameString.GL_DELETE_BUFFERS:
                return hookGlDeleteBuffers(index);
            case FuncNameString.GL_GEN_FRAMEBUFFERS:
                return hookGlGenFramebuffers(index);
            case FuncNameString.GL_DELETE_FRAMEBUFFERS:
                return hookGlDeleteFramebuffers(index);
            case FuncNameString.GL_GEN_RENDERBUFFERS:
                return hookGlGenRenderbuffers(index);
            case FuncNameString.GL_DELETE_RENDERBUFFERS:
                return hookGlDeleteRenderbuffers(index);
            case FuncNameString.GL_TEX_IMAGE_2D:
                return hookGlTexImage2D(index);
            case FuncNameString.GL_TEX_IMAGE_3D:
                return hookGlTexImage3D(index);
            case FuncNameString.GL_BIND_TEXTURE:
                return hookGlBindTexture(index);
            case FuncNameString.GL_BIND_BUFFER:
                return hookGlBindBuffer(index);
            case FuncNameString.GL_BIND_FRAMEBUFFER:
                return hookGlBindFramebuffer(index);
            case FuncNameString.GL_BIND_RENDERBUFFER:
                return hookGlBindRenderbuffer(index);
            case FuncNameString.GL_BUFFER_DATA:
                return hookGlBufferData(index);
            case FuncNameString.GL_RENDER_BUFFER_STORAGE:
                return hookGlRenderbufferStorage(index);
        }

        return false;
    }

    public native boolean init();

    public native void setNativeStackDump(boolean open);

    public native void setJavaStackDump(boolean open);

    private static native boolean hookGlGenTextures(int index);

    private static native boolean hookGlDeleteTextures(int index);

    private static native boolean hookGlGenBuffers(int index);

    private static native boolean hookGlDeleteBuffers(int index);

    private static native boolean hookGlGenFramebuffers(int index);

    private static native boolean hookGlDeleteFramebuffers(int index);

    private static native boolean hookGlGenRenderbuffers(int index);

    private static native boolean hookGlDeleteRenderbuffers(int index);

    private static native boolean hookGlGetError(int index);

    private static native boolean hookGlTexImage2D(int index);

    private static native boolean hookGlTexImage3D(int index);

    private static native boolean hookGlBindTexture(int index);

    private static native boolean hookGlBindBuffer(int index);

    private static native boolean hookGlBindFramebuffer(int index);

    private static native boolean hookGlBindRenderbuffer(int index);

    private static native boolean hookGlBufferData(int index);

    private static native boolean hookGlRenderbufferStorage(int index);

    public static void onGlGenTextures(int[] ids, String threadId, String javaStack, long nativeStackPtr) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContextId, javaStack, nativeStackPtr, OP_GEN, LeakMonitor.getInstance().getCurrentActivityName(), counter);
                OpenGLResRecorder.getInstance().gen(openGLInfo);
                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenTextures(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteTextures(int[] ids, String threadId) {
        if (ids.length > 0) {

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContextId, OP_GEN);
                OpenGLResRecorder.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteTextures(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenBuffers(int[] ids, String threadId, String javaStack, long nativeStackPtr) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContextId, javaStack, nativeStackPtr, OP_GEN, LeakMonitor.getInstance().getCurrentActivityName(), counter);
                OpenGLResRecorder.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenBuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteBuffers(int[] ids, String threadId) {
        if (ids.length > 0) {

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContextId, OP_DELETE);
                OpenGLResRecorder.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteBuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenFramebuffers(int[] ids, String threadId, String javaStack, long nativeStackPtr) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContextId, javaStack, nativeStackPtr, OP_GEN, LeakMonitor.getInstance().getCurrentActivityName(), counter);
                OpenGLResRecorder.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenFramebuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteFramebuffers(int[] ids, String threadId) {
        if (ids.length > 0) {

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContextId, OP_DELETE);
                OpenGLResRecorder.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteFramebuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenRenderbuffers(int[] ids, String threadId, String javaStack, long nativeStackPtr) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContextId, javaStack, nativeStackPtr, OP_GEN, LeakMonitor.getInstance().getCurrentActivityName(), counter);
                OpenGLResRecorder.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenRenderbuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteRenderbuffers(int[] ids, String threadId) {
        if (ids.length > 0) {

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContextId, OP_DELETE);
                OpenGLResRecorder.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteRenderbuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlBindTexture(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.TEXTURE, target, eglContextId, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindTexture(target, id);
        }
    }

    public static void onGlBindBuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.BUFFER, target, eglContextId, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindBuffer(target, id);
        }
    }

    public static void onGlBindFramebuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.FRAME_BUFFERS, target, eglContextId, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindFramebuffer(target, id);
        }
    }

    public static void onGlBindRenderbuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.RENDER_BUFFERS, target, eglContextId, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindRenderbuffer(target, id);
        }
    }

    public static void onGlTexImage2D(int target, int level, int internalFormat, int width, int height, int border, int format, int type, long size, String javaStack, long nativeStack) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = BindCenter.getInstance().getCurrentResourceIdByTarget(OpenGLInfo.TYPE.TEXTURE, eglContextId, target);
        if (info == null) {
            MatrixLog.e(TAG, "onGlTexImage2D: getCurrentResourceIdByTarget result == null");
            return;
        }
        OpenGLInfo openGLInfo = OpenGLResRecorder.getInstance().getItemByEGLContextAndId(info.getType(), info.getEglContextNativeHandle(), info.getId());
        if (openGLInfo != null) {
            openGLInfo.setSize(size);
            OpenGLResRecorder.getInstance().replace(openGLInfo);
        }

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlTexImage2D(target, level, internalFormat, width, height, border, format, type, info.getId(), eglContextId, size, javaStack, OpenGLInfo.dumpNativeStack(nativeStack));
        }

    }

    public static void onGlTexImage3D(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, long size, String javaStack, long nativeStack) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = BindCenter.getInstance().getCurrentResourceIdByTarget(OpenGLInfo.TYPE.TEXTURE, eglContextId, target);
        if (info == null) {
            MatrixLog.e(TAG, "onGlTexImage3D: getCurrentResourceIdByTarget result == null");
            return;
        }
        OpenGLInfo openGLInfo = OpenGLResRecorder.getInstance().getItemByEGLContextAndId(info.getType(), info.getEglContextNativeHandle(), info.getId());
        if (openGLInfo != null) {
            openGLInfo.setSize(size);
            OpenGLResRecorder.getInstance().replace(openGLInfo);
        }

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, info.getId(), eglContextId, size, javaStack, OpenGLInfo.dumpNativeStack(nativeStack));
        }

    }

    public static void onGlBufferData(int target, long size, int usage, String javaStack, long nativeStack) {
        if (Thread.currentThread().getName().equals("RenderThread")) {
            return;
        }
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = BindCenter.getInstance().getCurrentResourceIdByTarget(OpenGLInfo.TYPE.BUFFER, eglContextId, target);
        if (info == null) {
            MatrixLog.e(TAG, "onGlBufferData: getCurrentResourceIdByTarget result == null");
            return;
        }
        OpenGLInfo openGLInfo = OpenGLResRecorder.getInstance().getItemByEGLContextAndId(info.getType(), info.getEglContextNativeHandle(), info.getId());
        if (openGLInfo != null) {
            openGLInfo.setSize(size);
            OpenGLResRecorder.getInstance().replace(openGLInfo);
        }

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlBufferData(target, usage, info.getId(), eglContextId, size, javaStack, OpenGLInfo.dumpNativeStack(nativeStack));
        }

    }

    public static void onGlRenderbufferStorage(int target, int internalformat, int width, int height, long size, String javaStack, long nativeStack) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = BindCenter.getInstance().getCurrentResourceIdByTarget(OpenGLInfo.TYPE.RENDER_BUFFERS, eglContextId, target);
        if (info == null) {
            MatrixLog.e(TAG, "onGlRenderbufferStorage: getCurrentResourceIdByTarget result == null");
            return;
        }
        OpenGLInfo openGLInfo = OpenGLResRecorder.getInstance().getItemByEGLContextAndId(info.getType(), info.getEglContextNativeHandle(), info.getId());
        if (openGLInfo != null) {
            openGLInfo.setSize(size);
            OpenGLResRecorder.getInstance().replace(openGLInfo);
        }

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlRenderbufferStorage(target, width, height, internalformat, info.getId(), eglContextId, size, javaStack, OpenGLInfo.dumpNativeStack(nativeStack));
        }
    }

    public static void onGetError(int eid) {
        if (getInstance().mErrorListener != null) {
            getInstance().mErrorListener.onGlError(eid);
        }
    }


    public interface ErrorListener {

        void onGlError(int eid);

    }

    public interface BindListener {

        void onGlBindTexture(int target, int id);

        void onGlBindBuffer(int target, int id);

        void onGlBindRenderbuffer(int target, int id);

        void onGlBindFramebuffer(int target, int id);

    }

    public interface MemoryListener {

        void onGlTexImage2D(int target, int level, int internalFormat, int width, int height, int border, int format, int type, int id, long eglContextId, long size, String javaStack, String nativeStack);

        void onGlTexImage3D(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size, String javaStack, String nativeStack);

        void onGlBufferData(int target, int usage, int id, long eglContextId, long size, String javaStack, String nativeStack);

        void onGlRenderbufferStorage(int target, int width, int height, int internalFormat, int id, long eglContextId, long size, String javaStack, String nativeStack);

    }

    public interface ResourceListener {

        void onGlGenTextures(OpenGLInfo info);

        void onGlDeleteTextures(OpenGLInfo info);

        void onGlGenBuffers(OpenGLInfo info);

        void onGlDeleteBuffers(OpenGLInfo info);

        void onGlGenFramebuffers(OpenGLInfo info);

        void onGlDeleteFramebuffers(OpenGLInfo info);

        void onGlGenRenderbuffers(OpenGLInfo info);

        void onGlDeleteRenderbuffers(OpenGLInfo info);
    }

    public static String getStack() {
        return stackTraceToString(new Throwable().getStackTrace());
    }

    private static String stackTraceToString(final StackTraceElement[] arr) {
        if (arr == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder();

        for (int i = 0; i < arr.length; i++) {
            if (i == 0) {
                continue;
            }
            StackTraceElement element = arr[i];
            String className = element.getClassName();
            // remove unused stacks
            if (className.contains("java.lang.Thread")) {
                continue;
            }
            sb.append("\t").append(element).append('\n');
        }
        return sb.toString();
    }

}
