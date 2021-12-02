package com.tencent.matrix.openglleak.statistics.resource;

import android.annotation.SuppressLint;
import android.os.Handler;

import com.tencent.matrix.openglleak.statistics.MemoryInfo;
import com.tencent.matrix.openglleak.utils.AutoWrapBuilder;
import com.tencent.matrix.openglleak.utils.GlLeakHandlerThread;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

public class ResRecordManager {

    private static final ResRecordManager mInstance = new ResRecordManager();

    private final Handler mH;

    private final List<Callback> mCallbackList = new LinkedList<>();
    private final List<OpenGLInfo> mInfoList = new LinkedList<>();

    private ResRecordManager() {
        mH = new Handler(GlLeakHandlerThread.getInstance().getLooper());
    }

    public static ResRecordManager getInstance() {
        return mInstance;
    }

    public void gen(final OpenGLInfo gen) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (gen == null) {
                    return;
                }

                synchronized (mInfoList) {
                    mInfoList.add(gen);
                }

                synchronized (mCallbackList) {
                    for (Callback cb : mCallbackList) {
                        if (null != cb) {
                            cb.gen(gen);
                        }
                    }
                }
            }
        });
    }

    public void delete(final OpenGLInfo del) {
        mH.post(new Runnable() {
            @Override
            public void run() {
                if (del == null) {
                    return;
                }

                synchronized (mInfoList) {
                    // 之前可能释放过
                    int index = mInfoList.indexOf(del);
                    if (-1 == index) {
                        return;
                    }

                    OpenGLInfo info = mInfoList.get(index);
                    if (null == info) {
                        return;
                    }

                    AtomicInteger counter = info.getCounter();
                    counter.set(counter.get() - 1);
                    if (counter.get() == 0) {
                        releaseNative(info.getNativeStackPtr());
                    }

                    mInfoList.remove(del);
                }

                synchronized (mCallbackList) {
                    for (Callback cb : mCallbackList) {
                        if (null != cb) {
                            cb.delete(del);
                        }
                    }
                }
            }
        });
    }

    public OpenGLInfo findOpenGLInfo(OpenGLInfo.TYPE type, long eglContextId, int openGLInfoId) {
        synchronized (mInfoList) {
            for (OpenGLInfo item : mInfoList) {
                if (item == null) {
                    break;
                }

                if (type == item.getType() && item.getEglContextNativeHandle() == eglContextId && item.getId() == openGLInfoId) {
                    return item;
                }
            }
        }
        return null;
    }

    public String getNativeStack(OpenGLInfo item) {
        synchronized (mInfoList) {
            // 之前可能释放过
            int index = mInfoList.indexOf(item);
            if (-1 == index) {
                return "res already released, can not get native stack";
            }

            String ret = "";

            OpenGLInfo info = mInfoList.get(index);
            if (info == null) {
                return ret;
            }
            long nativeStackPtr = info.getNativeStackPtr();
            if (nativeStackPtr != 0L) {
                ret = dumpNativeStack(nativeStackPtr);
            }

            return ret;
        }
    }

    public static native String dumpNativeStack(long nativeStackPtr);

    private native void releaseNative(long nativeStackPtr);

    protected void registerCallback(Callback callback) {
        if (null == callback) {
            return;
        }

        synchronized (mCallbackList) {
            if (mCallbackList.contains(callback)) {
                return;
            }

            mCallbackList.add(callback);
        }
    }

    protected void unregisterCallback(Callback callback) {
        if (null == callback) {
            return;
        }
        synchronized (mCallbackList) {
            mCallbackList.remove(callback);
        }
    }

    public boolean isGLInfoRelease(OpenGLInfo item) {
        synchronized (mInfoList) {
            return !mInfoList.contains(item);
        }
    }

    public void clear() {
        synchronized (mInfoList) {
            mInfoList.clear();
        }
    }

    public void dumpGLToFile(String filePath) {
        File targetFile = new File(filePath);

        if (targetFile.exists()) {
            targetFile.delete();
        }

        try {
            targetFile.createNewFile();
        } catch (IOException ignored) {
        }

        try (BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(targetFile)))) {
            writer.write(dumpGLToString());
        } catch (IOException ignored) {
        }
    }

    @SuppressLint("DefaultLocale")
    private String getResListString(List<OpenGLReportInfo> resList) {
        AutoWrapBuilder result = new AutoWrapBuilder();
        for (OpenGLReportInfo report : resList) {
            result.append(String.format(" alloc count = %d", report.getAllocCount()))
                    .append(String.format(" id = %s", report.getAllocIdList()))
                    .append(String.format(" activity = %s", report.innerInfo.getActivityInfo().name))
                    .append(String.format(" type = %s", report.innerInfo.getType()))
                    .append(String.format(" eglContext = %s", report.innerInfo.getEglContextNativeHandle()))
                    .append(String.format(" java stack = %s", report.innerInfo.getJavaStack()))
                    .append(String.format(" native stack = %s", report.innerInfo.getNativeStack()))
                    .append(report.innerInfo.getMemoryInfo() == null ? "" : getMemoryInfoStr(report.innerInfo.getMemoryInfo()))
                    .wrap();
        }
        return result.toString();
    }

    private String getMemoryInfoStr(MemoryInfo memory) {
        return memory.getParamsInfos() +
                "\n" +
                String.format(" memory java stack = %s", memory.getJavaStack()) +
                "\n" +
                String.format(" memory native stack = %s", memory.getNativeStack());
    }

    @SuppressLint("DefaultLocale")
    public String dumpGLToString() {
        Map<Long, OpenGLReportInfo> infoMap = new HashMap<>();
        for (int i = 0; i < mInfoList.size(); i++) {

            OpenGLInfo info = mInfoList.get(i);
            int javaHash = info.getJavaStack().hashCode();
            int nativeHash = info.getNativeStack().hashCode();

            MemoryInfo memoryInfo = info.getMemoryInfo();
            int memoryJavaHash = memoryInfo == null ? 0 : memoryInfo.getJavaStack().hashCode();
            int memoryNativeHash = memoryInfo == null ? 0 : info.getNativeStack().hashCode();

            long infoHash = javaHash + nativeHash + memoryNativeHash + memoryJavaHash;

            OpenGLReportInfo oldInfo = infoMap.get(infoHash);
            if (oldInfo == null) {
                OpenGLReportInfo openGLReportInfo = new OpenGLReportInfo(info);
                infoMap.put(infoHash, openGLReportInfo);
            } else {
                // resource part
                boolean isSameType = info.getType() == oldInfo.innerInfo.getType();
                boolean isSameThread = info.getThreadId().equals(oldInfo.innerInfo.getThreadId());
                boolean isSameEglContext = info.getEglContextNativeHandle() == oldInfo.innerInfo.getEglContextNativeHandle();
                boolean isSameActivity = info.getActivityInfo().equals(oldInfo.innerInfo.getActivityInfo());

                if (isSameType && isSameThread && isSameEglContext && isSameActivity) {
                    oldInfo.incAllocRecord(info.getId());
                    MemoryInfo oldMemoryInfo = oldInfo.innerInfo.getMemoryInfo();
                    if (oldMemoryInfo != null) {
                        oldMemoryInfo.appendParamsInfos(info.getMemoryInfo());
                    }
                    infoMap.put(infoHash, oldInfo);
                }
            }
        }

        List<OpenGLReportInfo> textureList = new ArrayList<>();
        List<OpenGLReportInfo> bufferList = new ArrayList<>();
        List<OpenGLReportInfo> framebufferList = new ArrayList<>();
        List<OpenGLReportInfo> renderbufferList = new ArrayList<>();

        for (OpenGLReportInfo reportInfo : infoMap.values()) {
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.TEXTURE) {
                textureList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.BUFFER) {
                bufferList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.FRAME_BUFFERS) {
                framebufferList.add(reportInfo);
            }
            if (reportInfo.innerInfo.getType() == OpenGLInfo.TYPE.RENDER_BUFFERS) {
                renderbufferList.add(reportInfo);
            }
        }

        Comparator<OpenGLReportInfo> comparator = new Comparator<OpenGLReportInfo>() {
            @Override
            public int compare(OpenGLReportInfo o1, OpenGLReportInfo o2) {
                return o2.getAllocCount() - o1.getAllocCount();
            }
        };

        Collections.sort(textureList, comparator);
        Collections.sort(bufferList, comparator);
        Collections.sort(framebufferList, comparator);
        Collections.sort(renderbufferList, comparator);

        AutoWrapBuilder builder = new AutoWrapBuilder();
        builder.appendDotted()
                .appendWithSpace(String.format("textures Count = %d", textureList.size()), 3)
                .appendWithSpace(String.format("buffer Count = %d", bufferList.size()), 3)
                .appendWithSpace(String.format("framebuffer Count = %d", framebufferList.size()), 3)
                .appendWithSpace(String.format("renderbuffer Count = %d", renderbufferList.size()), 3)
                .appendDotted()
                .appendWave()
                .appendWithSpace("texture part :", 3)
                .appendWave()
                .append(getResListString(textureList))
                .appendWave()
                .appendWithSpace("buffers part :", 3)
                .appendWave()
                .append(getResListString(bufferList))
                .appendWave()
                .appendWithSpace("renderbuffer part :", 3)
                .appendWave()
                .append(getResListString(renderbufferList))
                .wrap();

        return builder.toString();
    }

    interface Callback {
        void gen(OpenGLInfo res);

        void delete(OpenGLInfo res);
    }

}
