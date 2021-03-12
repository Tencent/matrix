package com.tencent.matrix.memorycanary.util;

import android.os.Build;
import android.os.Debug;

import com.tencent.matrix.util.MatrixLog;

import java.lang.reflect.Method;

/**
 * Created by astrozhou on 2018/9/19.
 */
public class DebugMemoryInfoUtil {
    private static final String TAG = "Matrix.DebugMemoryInfoUtil";
    private static Method getMemoryStatMethod = null;

    public static int getMemoryStat(String statName, final Debug.MemoryInfo memoryInfo) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return -1;
        }

        try {
            if (getMemoryStatMethod == null) {
                Class<?> ownerClass = Class.forName("android.os.Debug$MemoryInfo");

                Class[] argsClass = new Class[1];
                argsClass[0] = String.class;

                getMemoryStatMethod = ownerClass.getMethod("getMemoryStat", argsClass);
            }

            Object[] params = new Object[1];
            params[0] = statName;
            return Integer.parseInt((String) getMemoryStatMethod.invoke(memoryInfo, params));
        } catch (Exception e) {
            MatrixLog.e(TAG, e.toString());
        }

        return -1;
    }

    public static int getTotalUss(final Debug.MemoryInfo memoryInfo) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return -1;
        }

        return memoryInfo.dalvikPrivateDirty + memoryInfo.nativePrivateDirty + memoryInfo.otherPrivateDirty + memoryInfo.getTotalPrivateClean();
    }

}
