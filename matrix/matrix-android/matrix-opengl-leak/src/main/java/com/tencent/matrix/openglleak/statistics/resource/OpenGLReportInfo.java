package com.tencent.matrix.openglleak.statistics.resource;

import java.util.ArrayList;
import java.util.List;

public class OpenGLReportInfo {

    public final OpenGLInfo innerInfo;

    private final List<Integer> idList = new ArrayList<>();

    private final List<String> paramsList = new ArrayList<>();

    private long totalSize;

    private int allocCount = 1;

    public OpenGLReportInfo(OpenGLInfo innerInfo) {
        this.innerInfo = innerInfo;
        idList.add(innerInfo.getId());
        appendParamsInfos(innerInfo.getMemoryInfo());
    }

    public void appendParamsInfos(MemoryInfo memoryInfo) {
        if (memoryInfo == null) {
            return;
        }
        OpenGLInfo.TYPE resType = memoryInfo.getResType();
        if (resType == OpenGLInfo.TYPE.TEXTURE) {
            FaceInfo[] faces = memoryInfo.getFaces();
            for (FaceInfo faceInfo : faces) {
                if (faceInfo != null) {
                    paramsList.add(faceInfo.getParams());
                }
            }
        } else if (resType == OpenGLInfo.TYPE.BUFFER) {
            paramsList.add("MemoryInfo{" +
                    "target=" + memoryInfo.getTarget() +
                    ", id=" + memoryInfo.getId() +
                    ", eglContextNativeHandle='" + memoryInfo.getEglContextId() + '\'' +
                    ", usage=" + memoryInfo.getUsage() +
                    ", size=" + memoryInfo.getSize() +
                    '}');
        } else if (resType == OpenGLInfo.TYPE.RENDER_BUFFERS) {
            paramsList.add("MemoryInfo{" +
                    "target=" + memoryInfo.getTarget() +
                    ", id=" + memoryInfo.getId() +
                    ", eglContextNativeHandle='" + memoryInfo.getEglContextId() + '\'' +
                    ", internalFormat=" + memoryInfo.getInternalFormat() +
                    ", width=" + memoryInfo.getWidth() +
                    ", height=" + memoryInfo.getHeight() +
                    ", size=" + memoryInfo.getSize() +
                    '}');
        }

    }

    public String getParamsInfos() {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < paramsList.size(); i++) {
            result.append(" ")
                    .append(paramsList.get(i))
                    .append("\n");
        }
        return result.toString();
    }

    public void incAllocRecord(int id) {
        this.allocCount++;
        idList.add(id);
    }

    public void appendSize(long size) {
        this.totalSize += size;
    }

    public long getTotalSize() {
        return totalSize;
    }

    public String getAllocIdList() {
        return idList.toString();
    }

    public int getAllocCount() {
        return allocCount;
    }

}
