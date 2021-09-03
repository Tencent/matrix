package com.tencent.matrix.openglleak.statistics;

import com.tencent.matrix.openglleak.utils.ExecuteCenter;
import com.tencent.matrix.util.MatrixLog;

import java.util.HashMap;
import java.util.Map;

import static android.opengl.GLES20.GL_TEXTURE_2D;
import static android.opengl.GLES30.GL_TEXTURE_2D_ARRAY;
import static android.opengl.GLES30.GL_TEXTURE_3D;
import static javax.microedition.khronos.opengles.GL11ExtensionPack.GL_TEXTURE_CUBE_MAP;

public class BindMap {

    private static final String TAG = "matrix.BindMap";

    private static final BindMap mInstance = new BindMap();

    private final Map<Long, Map<Integer, OpenGLInfo>> bindTextureMap;
    private final Map<Long, Map<Integer, OpenGLInfo>> bindBufferMap;
    private final Map<Long, Map<Integer, OpenGLInfo>> bindFramebufferMap;
    private final Map<Long, Map<Integer, OpenGLInfo>> bindRenderbufferMap;

    static BindMap getInstance() {
        return mInstance;
    }

    private BindMap() {
        bindTextureMap = new HashMap<>();
        bindBufferMap = new HashMap<>();
        bindFramebufferMap = new HashMap<>();
        bindRenderbufferMap = new HashMap<>();
    }

    private OpenGLInfo getBindTextureInfo(long eglContextId, int target) {
        synchronized (bindTextureMap) {
            Map<Integer, OpenGLInfo> subTextureMap = bindTextureMap.get(eglContextId);
            if (subTextureMap == null) {
                subTextureMap = new HashMap<>();
                bindTextureMap.put(eglContextId, subTextureMap);
            }
            return subTextureMap.get(target);
        }
    }

    private OpenGLInfo getBindBufferInfo(long eglContextId, int target) {
        synchronized (bindBufferMap) {
            Map<Integer, OpenGLInfo> subBufferMap = bindBufferMap.get(eglContextId);
            if (subBufferMap == null) {
                subBufferMap = new HashMap<>();
            }
            return subBufferMap.get(target);
        }
    }

    private OpenGLInfo getBindFramebufferInfo(long eglContextId, int target) {
        synchronized (bindFramebufferMap) {
            Map<Integer, OpenGLInfo> subFramebufferMap = bindFramebufferMap.get(eglContextId);
            if (subFramebufferMap == null) {
                subFramebufferMap = new HashMap<>();
            }
            return subFramebufferMap.get(target);
        }
    }

    private OpenGLInfo getBindRenderbufferInfo(long eglContextId, int target) {
        synchronized (bindRenderbufferMap) {
            Map<Integer, OpenGLInfo> subRenderbufferMap = bindRenderbufferMap.get(eglContextId);
            if (subRenderbufferMap == null) {
                subRenderbufferMap = new HashMap<>();
            }
            return subRenderbufferMap.get(target);
        }
    }

    private void putInBindTextureMap(final long eglContext, final int target, final OpenGLInfo info) {
        if (!isSupportTargetOfTexture(target)) {
            MatrixLog.e(TAG, "putInBindTextureMap input un support target = %d", target);
            return;
        }
        synchronized (bindTextureMap) {
            Map<Integer, OpenGLInfo> subTextureMap = bindTextureMap.get(eglContext);
            if (subTextureMap == null) {
                subTextureMap = new HashMap<>();
                bindTextureMap.put(eglContext, subTextureMap);
            }
            subTextureMap.put(target, info);
        }
    }

    private boolean isSupportTarget(OpenGLInfo.TYPE type, int target) {
        if (type == OpenGLInfo.TYPE.TEXTURE) {
            return isSupportTargetOfTexture(target);
        }
        if (type == OpenGLInfo.TYPE.BUFFER) {
            return isSupportTargetOfBuffer(target);
        }
        if (type == OpenGLInfo.TYPE.FRAME_BUFFERS) {
            return isSupportTargetOfFramebuffer(target);
        }
        if (type == OpenGLInfo.TYPE.RENDER_BUFFERS) {
            return isSupportTargetOfRenderbuffer(target);
        }
        return false;
    }

    private boolean isSupportTargetOfTexture(int target) {
        return target == GL_TEXTURE_2D || target == GL_TEXTURE_3D
                || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP;
    }

    private boolean isSupportTargetOfBuffer(int target) {
        return false;
    }

    private boolean isSupportTargetOfFramebuffer(int target) {
        return false;
    }

    private boolean isSupportTargetOfRenderbuffer(int target) {
        return false;
    }

    public OpenGLInfo getBindInfo(OpenGLInfo.TYPE type, long eglContextId, int target) {
        switch (type) {
            case BUFFER:
                return getBindBufferInfo(eglContextId, target);
            case TEXTURE:
                return getBindTextureInfo(eglContextId, target);
            case FRAME_BUFFERS:
                return getBindFramebufferInfo(eglContextId, target);
            case RENDER_BUFFERS:
                return getBindRenderbufferInfo(eglContextId, target);
        }
        return null;
    }


    public void putBindInfo(final OpenGLInfo.TYPE type, final long eglContextId, final int target, final OpenGLInfo info) {
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                switch (type) {
                    case BUFFER:
                        putInBindBufferMap(eglContextId, target, info);
                        break;
                    case TEXTURE:
                        putInBindTextureMap(eglContextId, target, info);
                        break;
                    case FRAME_BUFFERS:
                        putInBindFramebufferMap(eglContextId, target, info);
                        break;
                    case RENDER_BUFFERS:
                        putInBindRenderbufferMap(eglContextId, target, info);
                        break;
                }
            }
        });
    }

    private void putInBindRenderbufferMap(long eglContextId, int target, OpenGLInfo info) {
        //todo
    }

    private void putInBindFramebufferMap(long eglContextId, int target, OpenGLInfo info) {
        //todo
    }

    private void putInBindBufferMap(long eglContextId, int target, OpenGLInfo info) {
        //todo
    }

}
