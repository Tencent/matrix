package com.tencent.matrix.trace.extension;

public class MatrixTraceExtension {
    boolean transformInjectionForced;
    String baseMethodMapFile;
    String blackListFile;
    String customDexTransformName;
    boolean skipCheckClass = true; // skip by default

    boolean enable;

    public void setEnable(boolean enable) {
        this.enable = enable;
    }

    public void setBlackListFile(String blackListFile) {
        this.blackListFile = blackListFile;
    }

    public void setCustomDexTransformName(String customDexTransformName) {
        this.customDexTransformName = customDexTransformName;
    }

    public void setBaseMethodMapFile(String baseMethodMapFile) {
        this.baseMethodMapFile = baseMethodMapFile;
    }

    public void setTransformInjectionForced(boolean transformInjectionForced) {
        this.transformInjectionForced = transformInjectionForced;
    }

    public void setSkipCheckClass(boolean skipCheckClass) {
        this.skipCheckClass = skipCheckClass;
    }

    public String getBaseMethodMapFile() {
        return baseMethodMapFile;
    }

    public String getBlackListFile() {
        return blackListFile;
    }

    public String getCustomDexTransformName() {
        return customDexTransformName;
    }

    public boolean isTransformInjectionForced() {
        return transformInjectionForced;
    }

    public boolean isEnable() {
        return enable;
    }

    public boolean isSkipCheckClass() {
        return skipCheckClass;
    }
}
