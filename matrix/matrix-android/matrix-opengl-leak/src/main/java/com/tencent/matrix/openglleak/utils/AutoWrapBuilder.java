package com.tencent.matrix.openglleak.utils;

public class AutoWrapBuilder {

    private final StringBuilder stringBuilder;

    private static final String dottedLine = "-------------------------------------------------------------------------";
    private static final String waveLine = "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

    public AutoWrapBuilder() {
        stringBuilder = new StringBuilder();
    }

    public AutoWrapBuilder append(String content) {
        stringBuilder.append(content)
                .append("\n");
        return this;
    }

    @Override
    public String toString() {
        return stringBuilder.toString();
    }

    public AutoWrapBuilder appendDotted() {
        stringBuilder.append(dottedLine)
                .append("\n");
        return this;
    }

    public AutoWrapBuilder appendWave() {
        stringBuilder.append(waveLine)
                .append("\n");
        return this;
    }

    public AutoWrapBuilder wrap() {
        stringBuilder.append("\n");
        return this;
    }

    public AutoWrapBuilder appendWithSpace(String content, int count) {
        if (count > 0) {
            for (int i = 0; i < count; i++) {
                stringBuilder.append("\t");
            }
        }
        stringBuilder.append(content)
                .append("\n");
        return this;
    }

}
