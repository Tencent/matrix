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

    private static OpenGLHook mInstance = new OpenGLHook();
    private Listener mListener;

    private OpenGLHook() {
    }

    public static OpenGLHook getInstance() {
        return mInstance;
    }

    public void setListener(Listener l) {
        mListener = l;
    }

    public boolean hook(String targetFuncName, int index) {
        if (targetFuncName.equals(FuncNameString.GL_GET_ERROR)) {
            return hookGlGetError(index);
        } else if (targetFuncName.equals(FuncNameString.GL_GEN_TEXTURES)) {
            return hookGlGenTextures(index);
        } else if (targetFuncName.equals(FuncNameString.GL_DELETE_TEXTURES)) {
            return hookGlDeleteTextures(index);
        } else if (targetFuncName.equals(FuncNameString.GL_GEN_BUFFERS)) {
            return hookGlGenBuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_DELETE_BUFFERS)) {
            return hookGlDeleteBuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_GEN_FRAMEBUFFERS)) {
            return hookGlGenFramebuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_DELETE_FRAMEBUFFERS)) {
            return hookGlDeleteFramebuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_GEN_RENDERBUFFERS)) {
            return hookGlGenRenderbuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_DELETE_RENDERBUFFERS)) {
            return hookGlDeleteRenderbuffers(index);
        } else if (targetFuncName.equals(FuncNameString.GL_TEX_IMAGE_2D)) {
            return hookGlTexImage2D(index);
        } else if (targetFuncName.equals(FuncNameString.GL_BIND_TEXTURE)) {
            return hookGlBindTexture(index);
        } else if (targetFuncName.equals(FuncNameString.GL_BIND_BUFFER)) {
            return hookGlBindBuffer(index);
        } else if (targetFuncName.equals(FuncNameString.GL_BIND_FRAMEBUFFER)) {
            return hookGlBindFramebuffer(index);
        } else if (targetFuncName.equals(FuncNameString.GL_BIND_RENDERBUFFER)) {
            return hookGlBindRenderbuffer(index);
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

    private static native boolean hookGlBindTexture(int index);

    private static native boolean hookGlBindBuffer(int index);

    private static native boolean hookGlBindFramebuffer(int index);

    private static native boolean hookGlBindRenderbuffer(int index);

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
                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlGenTextures(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlDeleteTextures(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlGenBuffers(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlDeleteBuffers(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlGenFramebuffers(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlDeleteFramebuffers(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlGenRenderbuffers(openGLInfo);
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

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlDeleteRenderbuffers(openGLInfo);
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
    }

    public static void onGlBindBuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.BUFFER, target, eglContextId, info);
    }

    public static void onGlBindFramebuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.FRAME_BUFFERS, target, eglContextId, info);
    }

    public static void onGlBindRenderbuffer(int target, int id) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, eglContextId, OP_BIND);
        BindCenter.getInstance().glBindResource(OpenGLInfo.TYPE.RENDER_BUFFERS, target, eglContextId, info);
    }

    public static void onGlTexImage2D(int target, long size) {
        long eglContextId = 0L;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
        }
        OpenGLInfo info = BindCenter.getInstance().getCurrentResourceIdByTarget(OpenGLInfo.TYPE.TEXTURE, eglContextId, target);
        if (info == null) {
            MatrixLog.e(TAG, "getCurrentResourceIdByTarget result == null");
            return;
        }
        OpenGLInfo openGLInfo = OpenGLResRecorder.getInstance().getItemByEGLContextAndId(info.getType(), info.getEglContextNativeHandle(), info.getId());
        if (openGLInfo != null) {
            openGLInfo.setSize(size);
            OpenGLResRecorder.getInstance().replace(openGLInfo);
        }
    }

    public static void onGetError(int eid) {
        if (getInstance().mListener != null) {
            getInstance().mListener.onGetError(new OpenGLInfo(eid));
        }
    }

    public interface Listener {
        void onGetError(OpenGLInfo info);

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
