package com.tencent.matrix.openglleak.statistics.resource;

import static android.opengl.GLES20.GL_TEXTURE_2D;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_X;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
import static android.opengl.GLES20.GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
import static android.opengl.GLES30.GL_TEXTURE_2D_ARRAY;
import static android.opengl.GLES30.GL_TEXTURE_3D;

import com.tencent.matrix.openglleak.hook.OpenGLHook;
import com.tencent.matrix.openglleak.utils.JavaStacktrace;
import com.tencent.matrix.util.MatrixLog;

import java.util.Arrays;

public class MemoryInfo {

    private int target;

    private int internalFormat;

    private int width;

    private int height;

    private int id;

    private long eglContextId;

    private int usage;

    private int backtraceKey;

    private long nativeStackPtr;

    private final OpenGLInfo.TYPE resType;

    // use for buffer & renderbuffer
    private long size;

    // only use for textures
    private FaceInfo[] faces;

    public MemoryInfo(OpenGLInfo.TYPE type) {
        this.resType = type;
        if (resType == OpenGLInfo.TYPE.TEXTURE) {
            final int cubeMapFaceCount = 6;
            faces = new FaceInfo[cubeMapFaceCount];
        }
    }

    public int getJavaStacktraceKey() {
        return backtraceKey;
    }

    private int getFaceId(int target) {
        switch (target) {
            case GL_TEXTURE_2D:
            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
                return 0;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
                return 1;
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
                return 2;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
                return 3;
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
                return 4;
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                return 5;
            default:
                return -1;
        }
    }

    public void releaseNativeStackPtr() {
        nativeStackPtr = 0;
    }

    public void setTexturesInfo(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size, int key, long nativeStackPtr) {
        int faceId = getFaceId(target);
        if (faceId == -1) {
            MatrixLog.e("MicroMsg.OpenGLHook", "setTexturesInfo faceId = -1, target = " + target);
            return;
        }

        if (this.nativeStackPtr != 0) {
            OpenGLHook.releaseNative(this.nativeStackPtr);
        }

        this.backtraceKey = key;
        this.nativeStackPtr = nativeStackPtr;

        FaceInfo faceInfo = faces[faceId];
        if (faceInfo == null) {
            faceInfo = new FaceInfo();
        }

        faceInfo.setTarget(target);
        faceInfo.setId(id);
        faceInfo.setEglContextNativeHandle(eglContextId);
        faceInfo.setLevel(level);
        faceInfo.setInternalFormat(internalFormat);
        faceInfo.setWidth(width);
        faceInfo.setHeight(height);
        faceInfo.setDepth(depth);
        faceInfo.setBorder(border);
        faceInfo.setFormat(format);
        faceInfo.setType(type);
        faceInfo.setSize(size);

        faces[faceId] = faceInfo;
    }

    public OpenGLInfo.TYPE getResType() {
        return resType;
    }

    public String getJavaStack() {
        return JavaStacktrace.getBacktraceValue(backtraceKey);
    }

    public String getNativeStack() {
        return nativeStackPtr != 0 ? OpenGLHook.dumpNativeStack(nativeStackPtr) : "";
    }

    public long getNativeStackPtr() {
        return nativeStackPtr;
    }

    public int getTarget() {
        return target;
    }

    public int getInternalFormat() {
        return internalFormat;
    }

    public int getWidth() {
        return width;
    }

    public int getHeight() {
        return height;
    }

    public int getId() {
        return id;
    }

    public FaceInfo[] getFaces() {
        return faces;
    }

    public long getSize() {
        long actualSize = 0L;
        if (this.resType == OpenGLInfo.TYPE.TEXTURE) {
            for (FaceInfo faceInfo : faces) {
                if (faceInfo != null) {
                    actualSize += faceInfo.getSize();
                }
            }
        } else {
            actualSize = size;
        }
        return actualSize;
    }

    public long getEglContextId() {
        return eglContextId;
    }

    public int getUsage() {
        return usage;
    }


    public void setBufferInfo(int target, int usage, int id, long eglContextId, long size, int backtrace, long nativeStackPtr) {
        this.target = target;
        this.usage = usage;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.backtraceKey = backtrace;
        this.nativeStackPtr = nativeStackPtr;
    }

    public void setRenderbufferInfo(int target, int width, int height, int internalFormat, int id, long eglContextId, long size, int backtrace, long nativeStackPtr) {
        this.target = target;
        this.width = width;
        this.height = height;
        this.internalFormat = internalFormat;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.backtraceKey = backtrace;
        this.nativeStackPtr = nativeStackPtr;
    }

    @Override
    public String toString() {
        return "MemoryInfo{" +
                "target=" + target +
                ", internalFormat=" + internalFormat +
                ", width=" + width +
                ", height=" + height +
                ", id=" + id +
                ", eglContextId=" + eglContextId +
                ", usage=" + usage +
                ", javaStack='" + getJavaStack() + '\'' +
                ", nativeStack='" + getNativeStack() + '\'' +
                ", resType=" + resType +
                ", size=" + getSize() +
                ", faces=" + Arrays.toString(faces) +
                '}';
    }

}
