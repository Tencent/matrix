package com.tencent.matrix.openglleak.detector;

import com.tencent.matrix.openglleak.comm.FuncNameString;

class FuncSeeker {

    public static int getFuncIndex(String targetFuncName) {
        if (targetFuncName.equals(FuncNameString.GL_GET_ERROR)) {
            return getGlGetErrorIndex();
        } else if (targetFuncName.startsWith("glGen") || targetFuncName.startsWith("glDelete")) {
            return getTargetFuncIndex(targetFuncName);
        } else if (targetFuncName.startsWith("glBind")) {
            return getBindFuncIndex(targetFuncName);
        } else if (targetFuncName.equals("glTexImage2D")) {
            return getGlTexImage2DIndex();
        } else if (targetFuncName.equals("glTexImage3D")) {
            return getGlTexImage3DIndex();
        }

        return 0;
    }

    private static native int getGlGetErrorIndex();

    private static native int getTargetFuncIndex(String targetFuncName);

    private static native int getBindFuncIndex(String bindFuncName);

    private static native int getGlTexImage2DIndex();

    private static native int getGlTexImage3DIndex();

}
