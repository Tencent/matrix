package com.tencent.matrix.openglleak.detector;

import androidx.annotation.Keep;

import com.tencent.matrix.openglleak.comm.FuncNameString;

@Keep
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
        } else if (targetFuncName.equals("glBufferData")) {
            return getGlBufferDataIndex();
        } else if (targetFuncName.equals("glRenderbufferStorage")) {
            return getGlRenderbufferStorageIndex();
        }

        return 0;
    }

    private static native int getGlGetErrorIndex();

    private static native int getTargetFuncIndex(String targetFuncName);

    private static native int getBindFuncIndex(String bindFuncName);

    private static native int getGlTexImage2DIndex();

    private static native int getGlTexImage3DIndex();

    private static native int getGlBufferDataIndex();

    private static native int getGlRenderbufferStorageIndex();

}
