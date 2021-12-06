package com.tencent.matrix.openglleak.statistics;

import static android.opengl.GLES20.GL_ARRAY_BUFFER;
import static android.opengl.GLES20.GL_ELEMENT_ARRAY_BUFFER;
import static android.opengl.GLES20.GL_RENDERBUFFER;
import static android.opengl.GLES20.GL_TEXTURE_2D;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_X;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
import static android.opengl.GLES30.GL_COPY_READ_BUFFER;
import static android.opengl.GLES30.GL_COPY_WRITE_BUFFER;
import static android.opengl.GLES30.GL_PIXEL_PACK_BUFFER;
import static android.opengl.GLES30.GL_PIXEL_UNPACK_BUFFER;
import static android.opengl.GLES30.GL_TEXTURE_2D_ARRAY;
import static android.opengl.GLES30.GL_TEXTURE_3D;
import static android.opengl.GLES30.GL_TRANSFORM_FEEDBACK_BUFFER;
import static android.opengl.GLES30.GL_UNIFORM_BUFFER;
import static android.opengl.GLES31.GL_ATOMIC_COUNTER_BUFFER;
import static android.opengl.GLES31.GL_DISPATCH_INDIRECT_BUFFER;
import static android.opengl.GLES31.GL_DRAW_INDIRECT_BUFFER;
import static android.opengl.GLES31.GL_SHADER_STORAGE_BUFFER;
import static android.opengl.GLES31.GL_TEXTURE_2D_MULTISAMPLE;
import static android.opengl.GLES32.GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
import static android.opengl.GLES32.GL_TEXTURE_BUFFER;
import static android.opengl.GLES32.GL_TEXTURE_CUBE_MAP_ARRAY;
import static javax.microedition.khronos.opengles.GL11ExtensionPack.GL_TEXTURE_CUBE_MAP;
import static javax.microedition.khronos.opengles.GL11ExtensionPack.GL_TEXTURE_CUBE_MAP_NEGATIVE_X;

import com.tencent.matrix.openglleak.statistics.resource.OpenGLInfo;
import com.tencent.matrix.openglleak.utils.ExecuteCenter;

import java.util.HashMap;
import java.util.Map;

public class BindMap {

    private static final BindMap mInstance = new BindMap();

    private final Map<Long, Map<Integer, OpenGLInfo>> bindTextureMap;
    private final Map<Long, Map<Integer, OpenGLInfo>> bindBufferMap;
    // glRenderbufferStorage target only support GL_RENDERBUFFER
    private final Map<Long, OpenGLInfo> bindRenderbufferMap;

    static BindMap getInstance() {
        return mInstance;
    }

    private BindMap() {
        bindTextureMap = new HashMap<>();
        bindBufferMap = new HashMap<>();
        bindRenderbufferMap = new HashMap<>();
    }

    private OpenGLInfo getBindMapInfo(final Map<Long, Map<Integer, OpenGLInfo>> bindMap, long eglContextNativeHandle, int target) {
        synchronized (bindMap) {
            Map<Integer, OpenGLInfo> subTextureMap = bindMap.get(eglContextNativeHandle);
            if (subTextureMap == null) {
                subTextureMap = new HashMap<>();
                bindMap.put(eglContextNativeHandle, subTextureMap);
            }
            return subTextureMap.get(target);
        }
    }

    private void putInBindMap(final Map<Long, Map<Integer, OpenGLInfo>> bindMap, final int target, final long eglContextNativeHandle, final OpenGLInfo openGLInfo, OpenGLInfo.TYPE type) {
        synchronized (bindMap) {
            Map<Integer, OpenGLInfo> subTextureMap = bindMap.get(eglContextNativeHandle);
            if (subTextureMap == null) {
                subTextureMap = new HashMap<>();
                bindMap.put(eglContextNativeHandle, subTextureMap);
            }
            subTextureMap.put(target, openGLInfo);
        }
    }

    private boolean isSupportTarget(OpenGLInfo.TYPE type, int target) {
        if (type == OpenGLInfo.TYPE.TEXTURE) {
            return isSupportTargetOfTexture(target);
        }
        if (type == OpenGLInfo.TYPE.BUFFER) {
            return isSupportTargetOfBuffer(target);
        }
        if (type == OpenGLInfo.TYPE.RENDER_BUFFERS) {
            return isSupportTargetOfRenderbuffer(target);
        }
        return false;
    }

    private boolean isSupportTargetOfTexture(int target) {
        return target == GL_TEXTURE_2D || target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_CUBE_MAP;
    }

    private boolean isSupportTargetOfBuffer(int target) {
        boolean targetOnUnder31 = target == GL_ARRAY_BUFFER || target == GL_COPY_READ_BUFFER || target == GL_COPY_WRITE_BUFFER
                || target == GL_ELEMENT_ARRAY_BUFFER || target == GL_PIXEL_PACK_BUFFER || target == GL_PIXEL_UNPACK_BUFFER
                || target == GL_TRANSFORM_FEEDBACK_BUFFER || target == GL_UNIFORM_BUFFER;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            boolean targetOn31 = target == GL_ATOMIC_COUNTER_BUFFER || target == GL_DISPATCH_INDIRECT_BUFFER || target == GL_DRAW_INDIRECT_BUFFER
                    || target == GL_SHADER_STORAGE_BUFFER;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
                boolean targetOn32 = target == GL_TEXTURE_BUFFER;
                return targetOnUnder31 || targetOn31 || targetOn32;
            } else {
                return targetOnUnder31 || targetOn31;
            }
        }
        return targetOnUnder31;
    }

    private boolean isSupportTargetOfRenderbuffer(int target) {
        return target == GL_RENDERBUFFER;
    }

    public OpenGLInfo getBindInfo(OpenGLInfo.TYPE type, long eglContextId, int target) {
        switch (type) {
            case BUFFER:
                return getBindMapInfo(bindBufferMap, eglContextId, target);
            case TEXTURE:
                int actualTarget = target;
                if(is2DCubeMapTarget(target)) {
                    actualTarget = GL_TEXTURE_CUBE_MAP;
                }
                return getBindMapInfo(bindTextureMap, eglContextId, actualTarget);
            case RENDER_BUFFERS:
                return bindRenderbufferMap.get(eglContextId);
        }
        return null;
    }

    private boolean is2DCubeMapTarget(int target) {
        return target == GL_TEXTURE_CUBE_MAP_POSITIVE_X || target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
                target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
                target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z || target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    }

    public void putBindInfo(final OpenGLInfo.TYPE type, final int target, final long eglContextId, final OpenGLInfo info) {
        if (!isSupportTarget(type, target)) {
            return;
        }
        ExecuteCenter.getInstance().post(new Runnable() {
            @Override
            public void run() {
                switch (type) {
                    case BUFFER:
                        putInBindMap(bindBufferMap, target, eglContextId, info, OpenGLInfo.TYPE.BUFFER);
                        break;
                    case TEXTURE:
                        putInBindMap(bindTextureMap, target, eglContextId, info, OpenGLInfo.TYPE.TEXTURE);
                        break;
                    case RENDER_BUFFERS:
                        bindRenderbufferMap.put(eglContextId, info);
                        break;
                }
            }
        });
    }

}
