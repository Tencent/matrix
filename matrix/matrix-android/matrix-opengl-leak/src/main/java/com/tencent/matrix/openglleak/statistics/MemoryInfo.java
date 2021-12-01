package com.tencent.matrix.openglleak.statistics;

public class MemoryInfo {

    private int target;

    private int level;

    private int internalFormat;

    private int width;

    private int height;

    private int depth;

    private int border;

    private int format;

    private int type;

    private int id;

    private long size;

    private long eglContextId;

    private int usage;

    private String javaStack = "";

    private String nativeStack = "";

    private OpenGLInfo.TYPE resType;

    public MemoryInfo() {

    }

    public void setTexturesInfo(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size, String javaStack, String nativeStack) {
        this.target = target;
        this.level = level;
        this.internalFormat = internalFormat;
        this.width = width;
        this.height = height;
        this.depth = depth;
        this.border = border;
        this.format = format;
        this.type = type;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.javaStack = javaStack;
        this.nativeStack = nativeStack;
        resType = OpenGLInfo.TYPE.TEXTURE;
    }

    public String getJavaStack() {
        return javaStack;
    }

    public String getNativeStack() {
        return nativeStack;
    }

    public void setBufferInfo(int target, int usage, int id, long eglContextId, long size, String javaStack, String nativeStack) {
        this.target = target;
        this.usage = usage;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.javaStack = javaStack;
        this.nativeStack = nativeStack;
        resType = OpenGLInfo.TYPE.BUFFER;
    }

    public void setRenderbufferInfo(int target, int width, int height, int internalFormat, int id, long eglContextId, long size, String javaStack, String nativeStack) {
        this.target = target;
        this.width = width;
        this.height = height;
        this.internalFormat = internalFormat;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.javaStack = javaStack;
        this.nativeStack = nativeStack;
        resType = OpenGLInfo.TYPE.RENDER_BUFFERS;
    }

    @Override
    public String toString() {
        if (resType == OpenGLInfo.TYPE.TEXTURE) {
            return "MemoryInfo{" +
                    "target=" + target +
                    ", level=" + level +
                    ", internalFormat=" + internalFormat +
                    ", width=" + width +
                    ", height=" + height +
                    ", depth=" + depth +
                    ", border=" + border +
                    ", format=" + format +
                    ", type=" + type +
                    ", size=" + size +
                    ", javaStack='" + javaStack + '\'' +
                    ", nativeStack='" + nativeStack + '\'' +
                    '}';
        } else if (resType == OpenGLInfo.TYPE.BUFFER) {
            return "MemoryInfo{" +
                    "target=" + target +
                    ", usage=" + usage +
                    ", size=" + size +
                    ", javaStack='" + javaStack + '\'' +
                    ", nativeStack='" + nativeStack + '\'' +
                    '}';
        } else if (resType == OpenGLInfo.TYPE.RENDER_BUFFERS) {
            return "MemoryInfo{" +
                    "target=" + target +
                    ", internalFormat=" + internalFormat +
                    ", width=" + width +
                    ", height=" + height +
                    ", size=" + size +
                    ", javaStack='" + javaStack + '\'' +
                    ", nativeStack='" + nativeStack + '\'' +
                    '}';
        } else {
            return "";
        }
    }
}
