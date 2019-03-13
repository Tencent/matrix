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

package com.tencent.matrix.util;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
//import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Debug;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.RandomAccessFile;
import java.util.regex.Pattern;

/**
 * Created by caichongyang on 17/5/18.
 * about Device Info
 */
public class DeviceUtil {
    private static final long MB = 1024 * 1024;
    private static final String TAG = "Matrix.DeviceUtil";
    private static final int INVALID = 0;
    private static final String MEMORY_FILE_PATH = "/proc/meminfo";
    private static final String CPU_FILE_PATH_0 = "/sys/devices/system/cpu/";
    private static final String CPU_FILE_PATH_1 = "/sys/devices/system/cpu/possible";
    private static final String CPU_FILE_PATH_2 = "/sys/devices/system/cpu/present";
    private static LEVEL sLevelCache = null;

    public static final String DEVICE_MACHINE = "machine";
    private static final String DEVICE_MEMORY_FREE = "mem_free";
    private static final String DEVICE_MEMORY = "mem";
    private static final String DEVICE_CPU = "cpu_app";

    private static long sTotalMemory = 0;
    private static long sLowMemoryThresold = 0;
    private static int sMemoryClass = 0;

    public enum LEVEL {

        BEST(5), HIGH(4), MIDDLE(3), LOW(2), BAD(1), UN_KNOW(-1);

        int value;

        LEVEL(int val) {
            this.value = val;
        }

        public int getValue() {
            return value;
        }
    }

    public static JSONObject getDeviceInfo(JSONObject oldObj, Application context) {
        try {
            oldObj.put(DEVICE_MACHINE, getLevel(context));
            oldObj.put(DEVICE_CPU, getAppCpuRate());
            oldObj.put(DEVICE_MEMORY, getTotalMemory(context));
            oldObj.put(DEVICE_MEMORY_FREE, getMemFree());

        } catch (JSONException e) {
            MatrixLog.e(TAG, "[JSONException for stack, error: %s", e);
        }

        return oldObj;
    }

    public static LEVEL getLevel(Context context) {
        if (null != sLevelCache) {
            return sLevelCache;
        }
        long start = System.currentTimeMillis();
        long totalMemory = getTotalMemory(context);
        int coresNum = getNumOfCores();
        MatrixLog.i(TAG, "[getLevel] totalMemory:%s coresNum:%s", totalMemory, coresNum);
        if (totalMemory >= 4 * 1024 * MB) {
            sLevelCache = LEVEL.BEST;
        } else if (totalMemory >= 3 * 1024 * MB) {
            sLevelCache = LEVEL.HIGH;
        } else if (totalMemory >= 2 * 1024 * MB) {
            if (coresNum >= 4) {
                sLevelCache = LEVEL.HIGH;
            } else if (coresNum >= 2) {
                sLevelCache = LEVEL.MIDDLE;
            } else if (coresNum > 0) {
                sLevelCache = LEVEL.LOW;
            }
        } else if (totalMemory >= 1024 * MB) {
            if (coresNum >= 4) {
                sLevelCache = LEVEL.MIDDLE;
            } else if (coresNum >= 2) {
                sLevelCache = LEVEL.LOW;
            } else if (coresNum > 0) {
                sLevelCache = LEVEL.LOW;
            }
        } else if (0 <= totalMemory && totalMemory < 1024 * MB) {
            sLevelCache = LEVEL.BAD;
        } else {
            sLevelCache = LEVEL.UN_KNOW;
        }

        MatrixLog.i(TAG, "getLevel, cost:" + (System.currentTimeMillis() - start) + ", level:" + sLevelCache);
        return sLevelCache;
    }

    private static int getAppId() {
        return android.os.Process.myPid();
    }

    public static long getLowMemoryThresold(Context context) {
        if (0 != sLowMemoryThresold) {
            return sLowMemoryThresold;
        }

        getTotalMemory(context);
        return sLowMemoryThresold;
    }

    //in KB
    public static int getMemoryClass(Context context) {
        if (0 != sMemoryClass) {
            return sMemoryClass * 1024;
        }
        getTotalMemory(context);
        return sMemoryClass * 1024;
    }

    public static long getTotalMemory(Context context) {
        if (0 != sTotalMemory) {
            return sTotalMemory;
        }

        long start = System.currentTimeMillis();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            am.getMemoryInfo(memInfo);
            sTotalMemory = memInfo.totalMem;
            sLowMemoryThresold = memInfo.threshold;

            long memClass = Runtime.getRuntime().maxMemory();
            if (memClass == Long.MAX_VALUE) {
                sMemoryClass = am.getMemoryClass(); //if not set maxMemory, then is not large heap
            } else {
                sMemoryClass = (int) (memClass / MB);
            }
//            int isLargeHeap = (context.getApplicationInfo().flags | ApplicationInfo.FLAG_LARGE_HEAP);
//            if (isLargeHeap > 0) {
//                sMemoryClass = am.getLargeMemoryClass();
//            } else {
//                sMemoryClass = am.getMemoryClass();
//            }

            MatrixLog.i(TAG, "getTotalMemory cost:" + (System.currentTimeMillis() - start) + ", total_mem:" + sTotalMemory
                    + ", LowMemoryThresold:" + sLowMemoryThresold + ", Memory Class:" + sMemoryClass);
            return sTotalMemory;
        }
        return 0;
    }

    public static boolean isLowMemory(Context context) {
        ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        am.getMemoryInfo(memInfo);
        return memInfo.lowMemory;
    }

    //return in KB
    public static long getAvailMemory(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            ActivityManager.MemoryInfo memInfo = new ActivityManager.MemoryInfo();
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            am.getMemoryInfo(memInfo);
            return memInfo.availMem / 1024;
        } else {
            long availMemory = INVALID;
            BufferedReader bufferedReader = null;
            try {
                bufferedReader = new BufferedReader(new InputStreamReader(new FileInputStream(MEMORY_FILE_PATH), "UTF-8"));
                String line = bufferedReader.readLine();
                while (null != line) {
                    String[] args = line.split("\\s+");
                    if ("MemFree:".equals(args[0])) {
                        availMemory = Integer.parseInt(args[1]) * 1024L;
                        break;
                    } else {
                        line = bufferedReader.readLine();
                    }
                }

            } catch (Exception e) {
                MatrixLog.i(TAG, "[getAvailMemory] error! %s", e.toString());
            } finally {
                try {
                    if (null != bufferedReader) {
                        bufferedReader.close();
                    }
                } catch (Exception e) {
                    MatrixLog.i(TAG, "close reader %s", e.toString());
                }
            }
            return availMemory / 1024;
        }
    }

    public static long getMemFree() {
        long availMemory = INVALID;
        BufferedReader bufferedReader = null;
        try {
            bufferedReader = new BufferedReader(new InputStreamReader(new FileInputStream(MEMORY_FILE_PATH), "UTF-8"));
            String line = bufferedReader.readLine();
            while (null != line) {
                String[] args = line.split("\\s+");
                if ("MemFree:".equals(args[0])) {
                    availMemory = Integer.parseInt(args[1]) * 1024L;
                    break;
                } else {
                    line = bufferedReader.readLine();
                }
            }

        } catch (Exception e) {
            MatrixLog.i(TAG, "[getAvailMemory] error! %s", e.toString());
        } finally {
            try {
                if (null != bufferedReader) {
                    bufferedReader.close();
                }
            } catch (Exception e) {
                MatrixLog.i(TAG, "close reader %s", e.toString());
            }
        }
        return availMemory / 1024;
    }

    public static double getAppCpuRate() {
        long start = System.currentTimeMillis();
        long cpuTime = 0L;
        long appTime = 0L;
        double cpuRate = 0.0D;
        RandomAccessFile procStatFile = null;
        RandomAccessFile appStatFile = null;

        try {
            procStatFile = new RandomAccessFile("/proc/stat", "r");
            String procStatString = procStatFile.readLine();
            String[] procStats = procStatString.split(" ");
            cpuTime = Long.parseLong(procStats[2]) + Long.parseLong(procStats[3])
                    + Long.parseLong(procStats[4]) + Long.parseLong(procStats[5])
                    + Long.parseLong(procStats[6]) + Long.parseLong(procStats[7])
                    + Long.parseLong(procStats[8]);

        } catch (Exception e) {
            MatrixLog.i(TAG, "RandomAccessFile(Process Stat) reader fail, error: %s", e.toString());
        } finally {
            try {
                if (null != procStatFile) {
                    procStatFile.close();
                }

            } catch (Exception e) {
                MatrixLog.i(TAG, "close process reader %s", e.toString());
            }
        }

        try {
            appStatFile = new RandomAccessFile("/proc/" + getAppId() + "/stat", "r");
            String appStatString = appStatFile.readLine();
            String[] appStats = appStatString.split(" ");
            appTime = Long.parseLong(appStats[13]) + Long.parseLong(appStats[14]);
        } catch (Exception e) {
            MatrixLog.i(TAG, "RandomAccessFile(App Stat) reader fail, error: %s", e.toString());
        } finally {
            try {
                if (null != appStatFile) {
                    appStatFile.close();
                }
            } catch (Exception e) {
                MatrixLog.i(TAG, "close app reader %s", e.toString());
            }
        }

        if (0 != cpuTime) {
            cpuRate = ((double) (appTime) / (double) (cpuTime)) * 100D;
        }

        MatrixLog.i(TAG, "getAppCpuRate cost:" + (System.currentTimeMillis() - start) + ",rate:" + cpuRate);
        return cpuRate;
    }

    public static Debug.MemoryInfo getAppMemory(Context context) {
        try {
            // 统计进程的内存信息 totalPss
            ActivityManager activityManager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            final Debug.MemoryInfo[] memInfo = activityManager.getProcessMemoryInfo(new int[]{getAppId()});
            if (memInfo.length > 0) {
                return memInfo[0];
            }
        } catch (Exception e) {
            MatrixLog.i(TAG, "getProcessMemoryInfo fail, error: %s", e.toString());
        }
        return null;
    }

    private static int getNumOfCores() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.GINGERBREAD_MR1) {
            return 1;
        }
        int cores;
        try {
            cores = getCoresFromFile(CPU_FILE_PATH_1);
            if (cores == INVALID) {
                cores = getCoresFromFile(CPU_FILE_PATH_2);
            }
            if (cores == INVALID) {
                cores = getCoresFromCPUFiles(CPU_FILE_PATH_0);
            }
        } catch (Exception e) {
            cores = INVALID;
        }
        if (cores == INVALID) {
            cores = 1;
        }
        return cores;
    }

    private static int getCoresFromCPUFiles(String path) {
        File[] list = new File(path).listFiles(CPU_FILTER);
        return null == list ? 0 : list.length;
    }

    private static int getCoresFromFile(String file) {
        InputStream is = null;
        try {
            is = new FileInputStream(file);
            BufferedReader buf = new BufferedReader(new InputStreamReader(is, "UTF-8"));
            String fileContents = buf.readLine();
            buf.close();
            if (fileContents == null || !fileContents.matches("0-[\\d]+$")) {
                return INVALID;
            }
            String num = fileContents.substring(2);
            return Integer.parseInt(num) + 1;
        } catch (IOException e) {
            MatrixLog.i(TAG, "[getCoresFromFile] error! %s", e.toString());
            return INVALID;
        } finally {
            try {
                if (is != null) {
                    is.close();
                }
            } catch (IOException e) {
                MatrixLog.i(TAG, "[getCoresFromFile] error! %s", e.toString());
            }
        }
    }

    private static final FileFilter CPU_FILTER = new FileFilter() {
        @Override
        public boolean accept(File pathname) {
            return Pattern.matches("cpu[0-9]", pathname.getName());
        }
    };

    public static long getDalvikHeap() {
        Runtime runtime = Runtime.getRuntime();
        return (runtime.totalMemory() - runtime.freeMemory()) / 1024;   //in KB
    }

    public static long getNativeHeap() {
        return Debug.getNativeHeapAllocatedSize() / 1024;   //in KB
    }

    public static long getVmSize() {
        String status = String.format("/proc/%s/status", getAppId());
        try {
            String content = getStringFromFile(status).trim();
            String[] args = content.split("\n");
            if (args.length > 12) {
                String size = args[12].split(":")[1].trim();
                return Long.parseLong(size.split(" ")[0]); // in KB
            }
        } catch (Exception e) {
            return -1;
        }
        return -1;
    }

    protected static String convertStreamToString(InputStream is) throws Exception {
        BufferedReader reader = null;
        StringBuilder sb = new StringBuilder();
        try {
            reader = new BufferedReader(new InputStreamReader(is));
            String line = null;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
        } finally {
            if (null != reader) {
                reader.close();
            }
        }

        return sb.toString();
    }

    protected static String getStringFromFile(String filePath) throws Exception {
        File fl = new File(filePath);
        FileInputStream fin = null;
        String ret;
        try {
            fin = new FileInputStream(fl);
            ret = convertStreamToString(fin);
        } finally {
            if (null != fin) {
                fin.close();
            }
        }
        return ret;
    }

}
