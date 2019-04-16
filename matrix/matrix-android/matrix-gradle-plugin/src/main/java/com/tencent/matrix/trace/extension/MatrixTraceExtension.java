package com.tencent.matrix.trace.extension;

public class MatrixTraceExtension {
    boolean enable;
    String baseMethodMapFile;
    String blackListFile;
    String customDexTransformName;

    public String getBaseMethodMapFile() {
        return baseMethodMapFile;
    }

    public String getBlackListFile() {
        return blackListFile;
    }

    public String getCustomDexTransformName() {
        return customDexTransformName;
    }

    public boolean isEnable() {
        return enable;
    }
}
