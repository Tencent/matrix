/*
 * Tencent is pleased to support the open source community by making wechat-matrix available.
 * Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the BSD 3-Clause License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
