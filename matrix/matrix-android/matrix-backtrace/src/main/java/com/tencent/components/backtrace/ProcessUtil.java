package com.tencent.components.backtrace;

import android.app.ActivityManager;
import android.content.Context;

import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.InputStream;

public class ProcessUtil {

    private static String sProcessName = null;

    public static synchronized String getProcessNameByPid(final Context context) {
        if (sProcessName == null) {
            sProcessName = ProcessUtil.getProcessNameByPidImpl(
                    context, android.os.Process.myPid());
        }
        return sProcessName;
    }

    private static String getProcessNameByPidImpl(final Context context, final int pid) {
        if (context == null || pid <= 0) {
            return "";
        }
        try {
            final ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            for (final ActivityManager.RunningAppProcessInfo i : am.getRunningAppProcesses()) {
                if (i.pid == pid && i.processName != null && !i.processName.equals("")) {
                    return i.processName;
                }
            }
        } catch (Exception ignore) {
        }

        byte[] b = new byte[128];
        InputStream in = null;
        try {
            in = new BufferedInputStream(new FileInputStream("/proc/" + pid + "/cmdline"));
            int len = in.read(b);
            if (len > 0) {
                for (int i = 0; i < len; i++) {
                    if (b[i] > 128 || b[i] <= 0) {
                        len = i;
                        break;
                    }
                }
                return new String(b, 0, len);
            }

        } catch (Exception ignore) {
        } finally {
            try {
                if (in != null) {
                    in.close();
                }
            } catch (Exception e) {
            }

        }

        return "";
    }

    public static boolean isMainProcess(Context context) {
        String processName = getProcessNameByPid(context);
        return context.getPackageName().equalsIgnoreCase(processName);
    }
}
