package com.tencent.matrix.openglleak.statistics.resource;

public class FaceInfo {

    private long size;
    private int target;
    private int id;
    private long eglContextNativeHandle;
    private int level;
    private int internalFormat;
    private int width;
    private int height;
    private int depth;
    private int border;
    private int format;
    private int type;

    public int getTarget() {
        return target;
    }

    public void setTarget(int target) {
        this.target = target;
    }

    public int getId() {
        return id;
    }

    public void setId(int id) {
        this.id = id;
    }

    public long getEglContextNativeHandle() {
        return eglContextNativeHandle;
    }

    public void setEglContextNativeHandle(long eglContextNativeHandle) {
        this.eglContextNativeHandle = eglContextNativeHandle;
    }

    public int getLevel() {
        return level;
    }

    public void setLevel(int level) {
        this.level = level;
    }

    public int getInternalFormat() {
        return internalFormat;
    }

    public void setInternalFormat(int internalFormat) {
        this.internalFormat = internalFormat;
    }

    public int getWidth() {
        return width;
    }

    public void setWidth(int width) {
        this.width = width;
    }

    public int getHeight() {
        return height;
    }

    public void setHeight(int height) {
        this.height = height;
    }

    public int getDepth() {
        return depth;
    }

    public void setDepth(int depth) {
        this.depth = depth;
    }

    public int getBorder() {
        return border;
    }

    public void setBorder(int border) {
        this.border = border;
    }

    public int getFormat() {
        return format;
    }

    public void setFormat(int format) {
        this.format = format;
    }

    public int getType() {
        return type;
    }

    public void setType(int type) {
        this.type = type;
    }

    public FaceInfo() {

    }

    public long getSize() {
        return size;
    }

    public void setSize(long size) {
        this.size = size;
    }

    @Override
    public String toString() {
        return "FaceInfo{" +
                "size=" + size +
                ", target=" + target +
                ", id=" + id +
                ", eglContextNativeHandle=" + eglContextNativeHandle +
                ", level=" + level +
                ", internalFormat=" + internalFormat +
                ", width=" + width +
                ", height=" + height +
                ", depth=" + depth +
                ", border=" + border +
                ", format=" + format +
                ", type=" + type +
                '}';
    }
}
