package com.tencent.matrix.openglleak.hook;

import static android.opengl.GLES30.GL_PIXEL_UNPACK_BUFFER;

import com.tencent.matrix.openglleak.comm.FuncNameString;
import com.tencent.matrix.openglleak.statistics.BindCenter;
import com.tencent.matrix.openglleak.statistics.resource.MemoryInfo;
import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.resource.ResRecordManager;
import com.tencent.matrix.openglleak.utils.ActivityRecorder;
import com.tencent.matrix.openglleak.utils.JavaStacktrace;
import com.tencent.matrix.util.MatrixLog;

import java.util.concurrent.atomic.AtomicInteger;

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

    public static native String dumpNativeStack(long nativeStackPtr);

    public static native void releaseNative(long nativeStackPtr);

    public static void onGlGenTextures(int[] ids, final String threadId, final int throwable, final long nativeStackPtr, final long eglContext) {
        if (ids.length > 0) {
            final AtomicInteger counter = new AtomicInteger(ids.length);

            for (final int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContext, throwable, nativeStackPtr, ActivityRecorder.getInstance().getCurrentActivityInfo(), counter);
                ResRecordManager.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenTextures(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteTextures(int[] ids, String threadId, final long eglContext) {
        if (ids.length > 0) {
            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContext);
                ResRecordManager.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteTextures(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenBuffers(int[] ids, String threadId, final int throwable, long nativeStackPtr, final long eglContext) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContext, throwable, nativeStackPtr, ActivityRecorder.getInstance().getCurrentActivityInfo(), counter);
                ResRecordManager.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenBuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteBuffers(int[] ids, String threadId, final long eglContext) {
        if (ids.length > 0) {
            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContext);
                ResRecordManager.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteBuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenFramebuffers(int[] ids, String threadId, final int throwable, long nativeStackPtr, final long eglContext) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContext, throwable, nativeStackPtr, ActivityRecorder.getInstance().getCurrentActivityInfo(), counter);
                ResRecordManager.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenFramebuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteFramebuffers(int[] ids, String threadId, final long eglContext) {
        if (ids.length > 0) {
            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContext);
                ResRecordManager.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteFramebuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlGenRenderbuffers(int[] ids, String threadId, final int throwable, long nativeStackPtr, final long eglContext) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContext, throwable, nativeStackPtr, ActivityRecorder.getInstance().getCurrentActivityInfo(), counter);
                ResRecordManager.getInstance().gen(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlGenRenderbuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGlDeleteRenderbuffers(int[] ids, String threadId, final long eglContext) {
        if (ids.length > 0) {
            for (int id : ids) {
                final OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContext);
                ResRecordManager.getInstance().delete(openGLInfo);

                if (getInstance().mResourceListener != null) {
                    getInstance().mResourceListener.onGlDeleteRenderbuffers(openGLInfo);
                }
            }
        }
    }

    public static void onGetError(int eid) {
        if (getInstance().mErrorListener != null) {
            getInstance().mErrorListener.onGlError(eid);
        }
    }


    public static void onGlBindTexture(final int target, final int id, final long eglContext) {
        OpenGLInfo info = null;
        if (id != 0) {
            info = ResRecordManager.getInstance().findOpenGLInfo(OpenGLInfo.TYPE.TEXTURE, eglContext, id);
        }
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.TEXTURE, target, eglContext, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindTexture(target, eglContext, id);
        }
    }

    public static void onGlBindBuffer(final int target, final int id, final long eglContext) {
        OpenGLInfo info = null;
        if (id != 0) {
            info = ResRecordManager.getInstance().findOpenGLInfo(OpenGLInfo.TYPE.BUFFER, eglContext, id);
        }
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.BUFFER, target, eglContext, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindBuffer(target, eglContext, id);
        }
    }

    public static void onGlBindFramebuffer(final int target, final int id, final long eglContext) {
        OpenGLInfo info = null;
        if (id != 0) {
            info = ResRecordManager.getInstance().findOpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, eglContext, id);
        }
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.FRAME_BUFFERS, target, eglContext, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindFramebuffer(target, eglContext, id);
        }
    }

    public static void onGlBindRenderbuffer(final int target, final int id, final long eglContext) {
        OpenGLInfo info = null;
        if (id != 0) {
            info = ResRecordManager.getInstance().findOpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, eglContext, id);
        }
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.RENDER_BUFFERS, target, eglContext, info);

        if (getInstance().mBindListener != null) {
            getInstance().mBindListener.onGlBindRenderbuffer(target, eglContext, id);
        }
    }

    public static void onGlTexImage2D(final int target, final int level, final int internalFormat, final int width, final int height, final int border, final int format, final int type, final long size, final int throwable, final long nativeStack, final long eglContext) {
        final OpenGLInfo openGLInfo = BindCenter.getInstance().findCurrentResourceIdByTarget(OpenGLInfo.TYPE.TEXTURE, eglContext, target);
        if (openGLInfo == null) {
            MatrixLog.e(TAG, "onGlTexImage2D: getCurrentResourceIdByTarget openGLID == null, maybe undo glBindTextures()");
            return;
        }

        MemoryInfo memoryInfo = openGLInfo.getMemoryInfo();
        if (memoryInfo == null) {
            memoryInfo = new MemoryInfo(OpenGLInfo.TYPE.TEXTURE);
        }
        memoryInfo.setTexturesInfo(target, level, internalFormat, width, height, 0, border, format, type, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size, throwable, nativeStack);
        openGLInfo.setMemoryInfo(memoryInfo);

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlTexImage2D(target, level, internalFormat, width, height, border, format, type, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size);
        }
    }

    public static void onGlTexImage3D(final int target, final int level, final int internalFormat, final int width, final int height, final int depth, final int border, final int format, final int type, final long size, final int throwable, final long nativeStack, final long eglContext) {
        final OpenGLInfo openGLInfo = BindCenter.getInstance().findCurrentResourceIdByTarget(OpenGLInfo.TYPE.TEXTURE, eglContext, target);
        if (openGLInfo == null) {
            MatrixLog.e(TAG, "onGlTexImage3D: getCurrentResourceIdByTarget result == null, maybe undo glBindTextures()");
            return;
        }

        MemoryInfo memoryInfo = openGLInfo.getMemoryInfo();
        if (memoryInfo == null) {
            memoryInfo = new MemoryInfo(OpenGLInfo.TYPE.TEXTURE);
        }
        memoryInfo.setTexturesInfo(target, level, internalFormat, width, height, depth, border, format, type, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size, throwable, nativeStack);
        openGLInfo.setMemoryInfo(memoryInfo);

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlTexImage3D(target, level, internalFormat, width, height, depth, border, format, type, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size);
        }
    }

    public static void onGlBufferData(final int target, final int usage, final long size, final int throwable, final long nativeStack, final long eglContext) {
        final OpenGLInfo openGLInfo = BindCenter.getInstance().findCurrentResourceIdByTarget(OpenGLInfo.TYPE.BUFFER, eglContext, target);
        if (openGLInfo == null) {
            MatrixLog.e(TAG, "onGlBufferData: getCurrentResourceIdByTarget result == null, maybe undo glBindBuffer()");
            return;
        }

        long actualSize = -1;
        if (target == GL_PIXEL_UNPACK_BUFFER) {
            actualSize = size * 2;
        } else {
            actualSize = size;
        }
        MemoryInfo memoryInfo = openGLInfo.getMemoryInfo();
        if (memoryInfo == null) {
            memoryInfo = new MemoryInfo(OpenGLInfo.TYPE.BUFFER);
        }
        memoryInfo.setBufferInfo(target, usage, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), actualSize, throwable, nativeStack);
        openGLInfo.setMemoryInfo(memoryInfo);

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlBufferData(target, usage, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size);
        }

    }

    public static void onGlRenderbufferStorage(final int target, final int internalformat, final int width, final int height, final long size, final int key, final long nativeStack, final long eglContext) {
        final OpenGLInfo openGLInfo = BindCenter.getInstance().findCurrentResourceIdByTarget(OpenGLInfo.TYPE.RENDER_BUFFERS, eglContext, target);
        if (openGLInfo == null) {
            MatrixLog.e(TAG, "onGlRenderbufferStorage: getCurrentResourceIdByTarget result == null, maybe undo glBindRenderbuffer()");
            return;
        }

        MemoryInfo memoryInfo = openGLInfo.getMemoryInfo();
        if (memoryInfo == null) {
            memoryInfo = new MemoryInfo(OpenGLInfo.TYPE.RENDER_BUFFERS);
        }
        memoryInfo.setRenderbufferInfo(target, width, height, internalformat, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size, key, nativeStack);
        openGLInfo.setMemoryInfo(memoryInfo);

        if (getInstance().mMemoryListener != null) {
            getInstance().mMemoryListener.onGlRenderbufferStorage(target, width, height, internalformat, openGLInfo.getId(), openGLInfo.getEglContextNativeHandle(), size);
        }
    }

    public interface ErrorListener {

        void onGlError(int eid);

    }

    public interface BindListener {

        void onGlBindTexture(int target, long eglContextId, int id);

        void onGlBindBuffer(int target, long eglContextId, int id);

        void onGlBindRenderbuffer(int target, long eglContextId, int id);

        void onGlBindFramebuffer(int target, long eglContextId, int id);

    }

    public interface MemoryListener {

        void onGlTexImage2D(int target, int level, int internalFormat, int width, int height, int border, int format, int type, int id, long eglContextId, long size);

        void onGlTexImage3D(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size);

        void onGlBufferData(int target, int usage, int id, long eglContextId, long size);

        void onGlRenderbufferStorage(int target, int width, int height, int internalFormat, int id, long eglContextId, long size);

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

    public static int getThrowable() {
        return JavaStacktrace.getBacktraceKey();
    }

}
