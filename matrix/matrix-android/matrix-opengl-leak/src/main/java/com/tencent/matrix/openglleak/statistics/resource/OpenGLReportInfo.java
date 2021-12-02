package com.tencent.matrix.openglleak.statistics.resource;

import java.util.ArrayList;
import java.util.List;

public class OpenGLReportInfo {

    public final OpenGLInfo innerInfo;

    private final List<Integer> idList = new ArrayList<>();

    private int allocCount;

    public OpenGLReportInfo(OpenGLInfo innerInfo) {
        this.innerInfo = innerInfo;
        idList.add(innerInfo.getId());
    }

    public void incAllocRecord(int id) {
        this.allocCount++;
        idList.add(id);
    }

    public String getAllocIdList() {
        return idList.toString();
    }

    public int getAllocCount() {
        return allocCount;
    }

}
