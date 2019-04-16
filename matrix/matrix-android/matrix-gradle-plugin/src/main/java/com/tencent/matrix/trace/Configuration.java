package com.tencent.matrix.trace;

import com.tencent.matrix.javalib.util.FileUtil;
import com.tencent.matrix.javalib.util.Util;
import com.tencent.matrix.trace.retrace.MappingCollector;

import java.io.File;
import java.util.HashSet;

public class Configuration {

    public String packageName;
    public String mappingDir;
    public String baseMethodMapPath;
    public String methodMapFilePath;
    public String ignoreMethodMapFilePath;
    public String blackListFilePath;
    public String traceClassOut;
    public HashSet<String> blackSet = new HashSet<>();

    Configuration(String packageName, String mappingDir, String baseMethodMapPath, String methodMapFilePath,
                  String ignoreMethodMapFilePath, String blackListFilePath, String traceClassOut) {
        this.packageName = packageName;
        this.mappingDir = Util.nullAsNil(mappingDir);
        this.baseMethodMapPath = Util.nullAsNil(baseMethodMapPath);
        this.methodMapFilePath = Util.nullAsNil(methodMapFilePath);
        this.ignoreMethodMapFilePath = Util.nullAsNil(ignoreMethodMapFilePath);
        this.blackListFilePath = Util.nullAsNil(blackListFilePath);
        this.traceClassOut = Util.nullAsNil(traceClassOut);
    }

    public int parseBlackFile(MappingCollector processor) {
        File blackFile = new File(blackListFilePath);
        if (!blackFile.exists()) {
            return blackSet.size();
        }

        String blackStr = TraceBuildConstants.DEFAULT_BLACK_TRACE + FileUtil.readFileAsString(blackFile.getAbsolutePath());

        String[] blackArray = blackStr.trim().replace("/", ".").split("\n");

        if (blackArray != null) {
            for (String black : blackArray) {
                if (black.length() == 0) {
                    continue;
                }
                if (black.startsWith("#")) {
                    continue;
                }
                if (black.startsWith("[")) {
                    continue;
                }

                if (black.startsWith("-keepclass ")) {
                    black = black.replace("-keepclass ", "");
                    blackSet.add(processor.proguardClassName(black, black));
                } else if (black.startsWith("-keeppackage ")) {
                    black = black.replace("-keeppackage ", "");
                    blackSet.add(processor.proguardPackageName(black, black));
                }
            }
        }
        return blackSet.size();
    }

    @Override
    public String toString() {
        return "\n# Configuration" + "\n"
                + "|* packageName:\t" + packageName + "\n"
                + "|* mappingDir:\t" + mappingDir + "\n"
                + "|* baseMethodMapPath:\t" + baseMethodMapPath + "\n"
                + "|* methodMapFilePath:\t" + methodMapFilePath + "\n"
                + "|* ignoreMethodMapFilePath:\t" + ignoreMethodMapFilePath + "\n"
                + "|* blackListFilePath:\t" + blackListFilePath + "\n"
                + "|* traceClassOut:\t" + traceClassOut + "\n";
    }

    public static class Builder {

        public String packageName;
        public String mappingPath;
        public String baseMethodMap;
        public String methodMapFile;
        public String ignoreMethodMapFile;
        public String blackListFile;
        public String traceClassOut;

        public Builder setPackageName(String packageName) {
            this.packageName = packageName;
            return this;
        }

        public Builder setMappingPath(String mappingPath) {
            this.mappingPath = mappingPath;
            return this;
        }

        public Builder setBaseMethodMap(String baseMethodMap) {
            this.baseMethodMap = baseMethodMap;
            return this;
        }

        public Builder setTraceClassOut(String traceClassOut) {
            this.traceClassOut = traceClassOut;
            return this;
        }

        public Builder setMethodMapFilePath(String methodMapDir) {
            methodMapFile = methodMapDir;
            return this;
        }

        public Builder setIgnoreMethodMapFilePath(String methodMapDir) {
            ignoreMethodMapFile = methodMapDir;
            return this;
        }

        public Builder setBlackListFile(String blackListFile) {
            this.blackListFile = blackListFile;
            return this;
        }

        public Configuration build() {
            return new Configuration(packageName, mappingPath, baseMethodMap, methodMapFile, ignoreMethodMapFile, blackListFile, traceClassOut);
        }

    }
}
