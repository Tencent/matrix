package com.tencent.matrix.openglleak.statistics.resource;

import java.util.Objects;

public class OpenGLID {

    private final int id;

    private final long eglContextNativeHandle;

    public OpenGLID(long eglContextNativeHandle, int id) {
        this.id = id;
        this.eglContextNativeHandle = eglContextNativeHandle;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        OpenGLID openGLID = (OpenGLID) o;
        return id == openGLID.id && eglContextNativeHandle == openGLID.eglContextNativeHandle;
    }

    @Override
    public int hashCode() {
        return Objects.hash(id, eglContextNativeHandle);
    }

    public int getId() {
        return id;
    }

    public long getEglContextNativeHandle() {
        return eglContextNativeHandle;
    }

    @Override
    public String toString() {
        return "OpenGLID{" +
                "id=" + id +
                ", eglContextNativeHandle=" + eglContextNativeHandle +
                '}';
    }

}
