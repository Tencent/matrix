package com.tencent.matrix.openglleak.hook;

import android.opengl.EGL14;

import com.tencent.matrix.openglleak.comm.FuncNameString;
import com.tencent.matrix.openglleak.statistics.LeakMonitor;
import com.tencent.matrix.openglleak.statistics.OpenGLInfo;
import com.tencent.matrix.openglleak.statistics.OpenGLResRecorder;

import java.util.concurrent.atomic.AtomicInteger;

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

    public static void onGlGenTextures(int[] ids, String threadId, String javaStack, long nativeStackPtr) {
        if (ids.length > 0) {
            AtomicInteger counter = new AtomicInteger(ids.length);

            long eglContextId = 0L;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
                eglContextId = EGL14.eglGetCurrentContext().getNativeHandle();
            }

            for (int id : ids) {
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContextId, javaStack, nativeStackPtr, true, LeakMonitor.getInstance().getCurrentActivityName(), counter);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.TEXTURE, id, threadId, eglContextId, false);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContextId, javaStack, nativeStackPtr, true, LeakMonitor.getInstance().getCurrentActivityName(), counter);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.BUFFER, id, threadId, eglContextId, false);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContextId, javaStack, nativeStackPtr, true, LeakMonitor.getInstance().getCurrentActivityName(), counter);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.FRAME_BUFFERS, id, threadId, eglContextId, false);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContextId, javaStack, nativeStackPtr, true, LeakMonitor.getInstance().getCurrentActivityName(), counter);
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
                OpenGLInfo openGLInfo = new OpenGLInfo(OpenGLInfo.TYPE.RENDER_BUFFERS, id, threadId, eglContextId, false);
                OpenGLResRecorder.getInstance().delete(openGLInfo);

                if (getInstance().mListener != null) {
                    getInstance().mListener.onGlDeleteRenderbuffers(openGLInfo);
                }
            }
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
