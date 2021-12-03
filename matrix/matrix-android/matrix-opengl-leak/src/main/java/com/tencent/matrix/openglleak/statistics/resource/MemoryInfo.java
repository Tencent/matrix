package com.tencent.matrix.openglleak.statistics.resource;

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

    private long nativeStackPtr;

    private OpenGLInfo.TYPE resType;

    public MemoryInfo() {

    }

    public void setTexturesInfo(int target, int level, int internalFormat, int width, int height, int depth, int border, int format, int type, int id, long eglContextId, long size, String javaStack, long nativeStackPtr) {
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
        this.nativeStackPtr = nativeStackPtr;
        resType = OpenGLInfo.TYPE.TEXTURE;
    }

    public OpenGLInfo.TYPE getResType() {
        return resType;
    }

    public String getJavaStack() {
        return javaStack;
    }

    public String getNativeStack() {
        if (nativeStack.isEmpty() && nativeStackPtr != 0) {
            nativeStack = ResRecordManager.dumpNativeStack(nativeStackPtr);
        }
        return nativeStack;
    }

    public long getNativeStackPtr() {
        return nativeStackPtr;
    }

    public int getTarget() {
        return target;
    }

    public int getLevel() {
        return level;
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

    public int getDepth() {
        return depth;
    }

    public int getBorder() {
        return border;
    }

    public int getFormat() {
        return format;
    }

    public int getType() {
        return type;
    }

    public int getId() {
        return id;
    }

    public long getSize() {
        return size;
    }

    public long getEglContextId() {
        return eglContextId;
    }

    public int getUsage() {
        return usage;
    }



    public void setBufferInfo(int target, int usage, int id, long eglContextId, long size, String javaStack, long nativeStackPtr) {
        this.target = target;
        this.usage = usage;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        resType = OpenGLInfo.TYPE.BUFFER;
    }

    public void setRenderbufferInfo(int target, int width, int height, int internalFormat, int id, long eglContextId, long size, String javaStack, long nativeStackPtr) {
        this.target = target;
        this.width = width;
        this.height = height;
        this.internalFormat = internalFormat;
        this.id = id;
        this.eglContextId = eglContextId;
        this.size = size;
        this.javaStack = javaStack;
        this.nativeStackPtr = nativeStackPtr;
        resType = OpenGLInfo.TYPE.RENDER_BUFFERS;
    }


}
