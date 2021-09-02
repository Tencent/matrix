package com.tencent.matrix.openglleak.detector;

import com.tencent.matrix.openglleak.comm.FuncNameString;

class FuncSeeker {

    public static int getFuncIndex(String targetFuncName) {
        if (targetFuncName.equals(FuncNameString.GL_GET_ERROR)) {
            return getGlGetErrorIndex();
        } else if (targetFuncName.startsWith("glGen") || targetFuncName.startsWith("glDelete")) {
            return getTargetFuncIndex(targetFuncName);
        }

        return 0;
    }

    private static native int getGlGetErrorIndex();

    private static native int getTargetFuncIndex(String targetFuncName);

}
